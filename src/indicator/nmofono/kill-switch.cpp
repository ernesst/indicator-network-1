/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Antti Kaijanmäki <antti.kaijanmaki@canonical.com>
 *     Marcus Tomlinson <marcus.tomlinson@canonical.com>
 */

#include <nmofono/kill-switch.h>
#include <backend-utils.h>
#include <dbus-types.h>

#include <URfkillInterface.h>
#include <URfkillKillswitchInterface.h>

namespace nmofono
{

class KillSwitch::Private
{
public:
    enum class DeviceType
    {
        wlan = 1,
        bluetooth = 2,
        uwb = 3,
        wimax = 4,
        wwan = 5,
        gps = 6,
        fm = 7,
        nfc = 8
    };

    std::shared_ptr<OrgFreedesktopURfkillInterface> urfkill;
    std::shared_ptr<OrgFreedesktopURfkillKillswitchInterface> killSwitch;

    bool m_flightMode = false;

    Private(std::shared_ptr<OrgFreedesktopURfkillInterface> urfkill,
            std::shared_ptr<OrgFreedesktopURfkillKillswitchInterface> killSwitch)
        : urfkill(urfkill),
          killSwitch(killSwitch) {}
};


KillSwitch::KillSwitch(const QDBusConnection& systemBus)
{
    auto urfkill = std::make_shared<OrgFreedesktopURfkillInterface>(DBusTypes::URFKILL_BUS_NAME,
                                                                    DBusTypes::URFKILL_OBJ_PATH,
                                                                    systemBus);

    auto killSwitch = std::make_shared<OrgFreedesktopURfkillKillswitchInterface>(DBusTypes::URFKILL_BUS_NAME,
                                                                                 DBusTypes::URFKILL_WIFI_OBJ_PATH,
                                                                                 systemBus);

    connect(urfkill.get(), SIGNAL(FlightModeChanged(bool)), this, SLOT(setFlightMode(bool)));
    connect(killSwitch.get(), SIGNAL(StateChanged()), this, SIGNAL(stateChanged()));

    d.reset(new Private(urfkill, killSwitch));

    setFlightMode(urfkill->IsFlightMode());
}

KillSwitch::~KillSwitch()
{}

void KillSwitch::setFlightMode(bool flightMode)
{
    if (flightMode == d->m_flightMode)
    {
        return;
    }

    d->m_flightMode = flightMode;
    Q_EMIT flightModeChanged(flightMode);
}

void
KillSwitch::setBlock(bool block)
{
    if (!block && state() == State::hard_blocked)
    {
        qCritical() << __PRETTY_FUNCTION__ << "Killswitch is hard blocked.";
        return;
    }

    if (!block && state() != State::soft_blocked)
    {
        return;
    }

    if (block && state() != State::unblocked)
    {
        return;
    }

    try
    {
        if (!utils::getOrThrow(d->urfkill->Block(static_cast<uint>(Private::DeviceType::wlan), block)))
        {
            throw std::runtime_error("Failed to block killswitch");
        }
    }
    catch (std::exception &e)
    {
        qCritical() << __PRETTY_FUNCTION__ << e.what();
    }
}

KillSwitch::State KillSwitch::state() const
{
    int stateIndex = d->killSwitch->state();
    if (stateIndex >= static_cast<int>(State::first_) &&
        stateIndex <= static_cast<int>(State::last_))
    {
        return static_cast<KillSwitch::State>(stateIndex);
    }
    return KillSwitch::State::not_available;
}

bool KillSwitch::flightMode(bool enable)
{
    if (enable == d->m_flightMode)
    {
        return true;
    }

    try
    {
        return utils::getOrThrow(d->urfkill->FlightMode(enable));
    }
    catch (std::runtime_error& e)
    {
        qWarning() << __PRETTY_FUNCTION__ << ": " << QString::fromStdString(e.what());
        return false;
    }
}

bool KillSwitch::isFlightMode()
{
    return d->m_flightMode;
}

}
