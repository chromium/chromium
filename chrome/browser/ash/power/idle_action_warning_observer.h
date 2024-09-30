// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_OBSERVER_H_
#define CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_OBSERVER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace views {
class Widget;
}

namespace ash {

class IdleActionWarningDialogView;

// Listens for notifications that the idle action is imminent and shows a
// warning dialog to the user.
class IdleActionWarningObserver {
 public:
  IdleActionWarningObserver();

  IdleActionWarningObserver(const IdleActionWarningObserver&) = delete;
  IdleActionWarningObserver& operator=(const IdleActionWarningObserver&) =
      delete;

  ~IdleActionWarningObserver();

 private:
  class PowerManagerObserver;
  class WidgetObserver;

  // PowerManagerClient::Observer:
  void IdleActionImminent(base::TimeDelta time_until_idle_action);
  void IdleActionDeferred();
  void PowerChanged(const power_manager::PowerSupplyProperties& proto);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget);

  void HideDialogIfPresent();

  std::unique_ptr<PowerManagerObserver> power_manager_observer_;
  std::unique_ptr<WidgetObserver> widged_observer_;

  raw_ptr<IdleActionWarningDialogView> warning_dialog_ = nullptr;  // Not owned.

  // Used to derive the correct idle action (IdleActionAC/IdleActionBattery).
  bool on_battery_power_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_OBSERVER_H_
