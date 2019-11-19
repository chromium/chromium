// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/idle_action_warning_observer.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/power/idle_action_warning_dialog_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/prefs/pref_service.h"

namespace {

// DO NOT REORDER - used to report metrics.
enum class IdleLogoutWarningEvent {
  kShown = 0,
  kCanceled = 1,
  kMaxValue = kCanceled
};

void ReportMetricsForDemoMode(IdleLogoutWarningEvent event) {
  if (chromeos::DemoSession::IsDeviceInDemoMode())
    UMA_HISTOGRAM_ENUMERATION("DemoMode.IdleLogoutWarningEvent", event);
}

chromeos::PowerPolicyController::Action GetIdleAction(bool on_battery_power) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  int action;
  if (on_battery_power)
    action = prefs->GetInteger(ash::prefs::kPowerBatteryIdleAction);
  else
    action = prefs->GetInteger(ash::prefs::kPowerAcIdleAction);
  return static_cast<chromeos::PowerPolicyController::Action>(action);
}

}  // namespace

namespace chromeos {

IdleActionWarningObserver::IdleActionWarningObserver() {
  PowerManagerClient::Get()->AddObserver(this);
}

IdleActionWarningObserver::~IdleActionWarningObserver() {
  PowerManagerClient::Get()->RemoveObserver(this);
  if (warning_dialog_) {
    warning_dialog_->GetWidget()->RemoveObserver(this);
    warning_dialog_->CloseDialog();
  }
}

void IdleActionWarningObserver::IdleActionImminent(
    const base::TimeDelta& time_until_idle_action) {
  // Only display warning if idle action is to shut down or logout.
  PowerPolicyController::Action idle_action = GetIdleAction(on_battery_power_);
  if (idle_action != PowerPolicyController::ACTION_STOP_SESSION &&
      idle_action != PowerPolicyController::ACTION_SHUT_DOWN) {
    HideDialogIfPresent();
    return;
  }

  const base::TimeTicks idle_action_time =
      base::TimeTicks::Now() + time_until_idle_action;
  if (warning_dialog_) {
    warning_dialog_->Update(idle_action_time);
  } else {
    warning_dialog_ = new IdleActionWarningDialogView(idle_action_time);
    warning_dialog_->GetWidget()->AddObserver(this);
    ReportMetricsForDemoMode(IdleLogoutWarningEvent::kShown);
  }
}

void IdleActionWarningObserver::IdleActionDeferred() {
  HideDialogIfPresent();
}

void IdleActionWarningObserver::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  on_battery_power_ =
      proto.battery_state() ==
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING;
}

void IdleActionWarningObserver::OnWidgetClosing(views::Widget* widget) {
  DCHECK(warning_dialog_);
  DCHECK_EQ(widget, warning_dialog_->GetWidget());
  warning_dialog_->GetWidget()->RemoveObserver(this);
  warning_dialog_ = nullptr;
}

void IdleActionWarningObserver::HideDialogIfPresent() {
  if (warning_dialog_) {
    warning_dialog_->CloseDialog();
    ReportMetricsForDemoMode(IdleLogoutWarningEvent::kCanceled);
  }
}

}  // namespace chromeos
