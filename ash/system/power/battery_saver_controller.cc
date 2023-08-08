// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_saver_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/power_notification_controller.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace ash {

// static
const double BatterySaverController::kActivationChargePercent = 20.0;

BatterySaverController::BatterySaverController(PrefService* local_state)
    : local_state_(local_state),
      always_on_(features::IsBatterySaverAlwaysOn()),
      previously_plugged_in_(PowerStatus::Get()->IsMainsChargerConnected()) {
  power_status_observation_.Observe(PowerStatus::Get());

  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      prefs::kPowerBatterySaver,
      base::BindRepeating(&BatterySaverController::OnSettingsPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  // Restore state from the saved preference value.
  OnSettingsPrefChanged();
}

BatterySaverController::~BatterySaverController() = default;

// static
void BatterySaverController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kPowerBatterySaver, false);
}

void BatterySaverController::MaybeResetNotificationAvailability(
    features::BatterySaverNotificationBehavior experiment,
    const double battery_percent,
    const int battery_remaining_minutes) {
  if (battery_remaining_minutes >
      PowerNotificationController::kLowPowerMinutes) {
    low_power_crossed_ = false;
  }

  if (battery_percent > kActivationChargePercent) {
    threshold_crossed_ = false;
  }
}

void BatterySaverController::OnPowerStatusChanged() {
  if (always_on_) {
    SetBatterySaverState(true);
    return;
  }

  const auto* power_status = PowerStatus::Get();
  const bool active = power_status->IsBatterySaverActive();
  const bool on_AC_power = power_status->IsMainsChargerConnected();
  const bool on_USB_power = power_status->IsUsbChargerConnected();
  const bool on_line_power = power_status->IsLinePowerConnected();

  // Update Settings UI to reflect current BSM state.
  UpdateSettings(active);

  // If we don't have a time-to-empty, powerd is still thinking so don't
  // try to auto-enable.
  const absl::optional<int> remaining_minutes =
      GetRemainingMinutes(power_status);
  if (remaining_minutes == absl::nullopt) {
    return;
  }

  const int battery_remaining_minutes = remaining_minutes.value();
  const double battery_percent = power_status->GetBatteryPercent();

  const bool charger_unplugged = previously_plugged_in_ && !on_AC_power;

  const bool percent_breached_threshold =
      battery_percent <= kActivationChargePercent;
  const bool minutes_breached_threshold =
      battery_remaining_minutes <=
      PowerNotificationController::kLowPowerMinutes;
  const auto experiment = features::kBatterySaverNotificationBehavior.Get();

  // If we are charging and we go above any of the thresholds, we reset them.
  if (on_AC_power || on_USB_power || on_line_power) {
    MaybeResetNotificationAvailability(experiment, battery_percent,
                                       battery_remaining_minutes);
  }

  // Should we turn off battery saver?
  if (active && on_AC_power) {
    SetBatterySaverState(false);
    return;
  }

  const bool threshold_conditions_met =
      !on_AC_power && percent_breached_threshold &&
      !minutes_breached_threshold && (!threshold_crossed_ || charger_unplugged);

  const bool low_power_conditions_met =
      !on_AC_power && minutes_breached_threshold &&
      (!low_power_crossed_ || charger_unplugged);

  switch (experiment) {
    case features::kFullyAutoEnable:
      // Auto Enable when either the battery percentage is at or below
      // 20%/15mins.
      if (threshold_conditions_met) {
        threshold_crossed_ = true;
        if (!active) {
          SetBatterySaverState(true);
        }
      }

      if (low_power_conditions_met) {
        low_power_crossed_ = true;
        if (!active) {
          SetBatterySaverState(true);
        }
      }
      break;
    case features::kOptInThenAutoEnable:
      // In this case, we don't do anything when we get to
      // kActivationChargePercent. However, when we get to 15 minutes
      // remaining, we auto enable.
      if (low_power_conditions_met) {
        low_power_crossed_ = true;
        if (!active) {
          SetBatterySaverState(true);
        }
      }
      break;
    case features::kFullyOptIn:
      // In this case, we never auto-enable battery saver mode. Enabling
      // battery saver mode is handled either power notification buttons, or
      // manually toggling battery saver in the settings.
    default:
      break;
  }

  previously_plugged_in_ = on_AC_power;
}

void BatterySaverController::OnSettingsPrefChanged() {
  if (always_on_) {
    SetBatterySaverState(true);
    return;
  }

  // OS Settings has changed the pref, tell Power Manager.
  SetBatterySaverState(local_state_->GetBoolean(prefs::kPowerBatterySaver));
}

void BatterySaverController::SetStateForTesting(bool active) {
  SetBatterySaverState(active);
}

void BatterySaverController::DisplayBatterySaverModeDisabledToast() {
  ToastManager* toast_manager = ToastManager::Get();
  // `toast_manager` can be null when this function is called in the unit tests
  // due to initialization priority.
  if (toast_manager == nullptr) {
    return;
  }

  toast_manager->Show(ToastData(
      "battery_saver_mode_state_changed",
      ToastCatalogName::kBatterySaverDisabled,
      l10n_util::GetStringUTF16(IDS_ASH_BATTERY_SAVER_DISABLED_TOAST_TEXT),
      ToastData::kDefaultToastDuration, true));
}

bool BatterySaverController::UpdateSettings(bool active) {
  bool changed = false;
  if (active != local_state_->GetBoolean(prefs::kPowerBatterySaver)) {
    local_state_->SetBoolean(prefs::kPowerBatterySaver, active);
    changed = true;
  }
  return changed;
}

void BatterySaverController::SetBatterySaverState(bool active) {
  bool changed = UpdateSettings(active);
  if (active != PowerStatus::Get()->IsBatterySaverActive()) {
    power_manager::SetBatterySaverModeStateRequest request;
    request.set_enabled(active);
    chromeos::PowerManagerClient::Get()->SetBatterySaverModeState(request);
    changed = true;
  }

  if (changed && !active) {
    DisplayBatterySaverModeDisabledToast();
  }
}

absl::optional<int> BatterySaverController::GetRemainingMinutes(
    const PowerStatus* status) {
  const absl::optional<base::TimeDelta> remaining_time =
      status->GetBatteryTimeToEmpty();

  // Check that powerd actually provided an estimate. It doesn't if the battery
  // current is so close to zero that the estimate would be huge.
  if (!remaining_time) {
    return absl::nullopt;
  }

  return base::ClampRound(*remaining_time / base::Minutes(1));
}

void BatterySaverController::UpdateBatterySaverStateFromNotification(
    NotificationType notification_type,
    bool active) {
  // TODO(cwd): Implement metrics based on the notification type that called
  // this method. For example:
  //  FullyAutoEnable + kThreshold + off => User explicitly opted out at 20%.
  //  OptInAutoEnable + kThreshold + on => User explicitly opted in at 20%.
  //  FullyOptIn + kLowPower + on => User explicitly opted in at 15 mins left.
  // Handle this how you like: either a map, switch statements, etc.
  // users_opt_status[features::kBatterySaverNotificationBehavior.Get()]
  //                 [notification_type] = active;

  // Update Battery Saver.
  SetBatterySaverState(active);
}

}  // namespace ash
