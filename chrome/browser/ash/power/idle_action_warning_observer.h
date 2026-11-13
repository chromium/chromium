// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_OBSERVER_H_
#define CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_OBSERVER_H_

#include <memory>

#include "base/time/time.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/views/widget/widget.h"

namespace ash {

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

  // PowerManagerClient::Observer:
  void IdleActionImminent(base::TimeDelta time_until_idle_action);
  void IdleActionDeferred();
  void PowerChanged(const power_manager::PowerSupplyProperties& proto);

  void OnWidgetClosed(views::Widget::ClosedReason reason);

  void HideDialogIfPresent();

  std::unique_ptr<PowerManagerObserver> power_manager_observer_;

  std::unique_ptr<views::Widget> warning_widget_;

  // Used to derive the correct idle action (IdleActionAC/IdleActionBattery).
  bool on_battery_power_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_OBSERVER_H_
