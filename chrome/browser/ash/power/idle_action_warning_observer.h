// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_OBSERVER_H_
#define CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_OBSERVER_H_

#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class IdleActionWarningDialogView;

// Listens for notifications that the idle action is imminent and shows a
// warning dialog to the user.
class IdleActionWarningObserver : public chromeos::PowerManagerClient::Observer,
                                  public views::WidgetObserver {
 public:
  IdleActionWarningObserver();

  IdleActionWarningObserver(const IdleActionWarningObserver&) = delete;
  IdleActionWarningObserver& operator=(const IdleActionWarningObserver&) =
      delete;

  ~IdleActionWarningObserver() override;

  // PowerManagerClient::Observer:
  void IdleActionImminent(base::TimeDelta time_until_idle_action) override;
  void IdleActionDeferred() override;
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

 private:
  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void HideDialogIfPresent();

  IdleActionWarningDialogView* warning_dialog_ = nullptr;  // Not owned.

  // Used to derive the correct idle action (IdleActionAC/IdleActionBattery).
  bool on_battery_power_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_IDLE_ACTION_WARNING_OBSERVER_H_
