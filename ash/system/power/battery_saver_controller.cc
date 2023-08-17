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
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
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
    SetState(true, UpdateReason::kAlwaysOn);
    return;
  }

  const auto* power_status = PowerStatus::Get();
  const bool active = power_status->IsBatterySaverActive();
  const bool on_AC_power = power_status->IsMainsChargerConnected();
  const bool on_USB_power = power_status->IsUsbChargerConnected();
  const bool on_line_power = power_status->IsLinePowerConnected();

  // Update Settings UI to reflect current BSM state.
  if (local_state_->GetBoolean(prefs::kPowerBatterySaver) != active) {
    SetState(active, UpdateReason::kPowerManager);
  }

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
    SetState(false, UpdateReason::kCharging);
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
          SetState(true, UpdateReason::kThreshold);
        }
      }

      if (low_power_conditions_met) {
        low_power_crossed_ = true;
        if (!active) {
          SetState(true, UpdateReason::kLowPower);
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
          SetState(true, UpdateReason::kLowPower);
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
    SetState(true, UpdateReason::kAlwaysOn);
    return;
  }

  // OS Settings has changed the pref, tell Power Manager.
  SetState(local_state_->GetBoolean(prefs::kPowerBatterySaver),
           UpdateReason::kSettings);
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

void BatterySaverController::SetState(bool active, UpdateReason reason) {
  auto* power_status = PowerStatus::Get();
  absl::optional<base::TimeDelta> time_to_empty =
      power_status->GetBatteryTimeToEmpty();
  double battery_percent = power_status->GetBatteryPercent();

  if (active == active_) {
    return;
  }
  active_ = active;

  // Update pref and Power Manager state.
  if (active != local_state_->GetBoolean(prefs::kPowerBatterySaver)) {
    // NB: This call is re-entrant. SetBoolean will call OnSettingsPrefChanged
    // which will call SetState recursively, which will exit early because
    // active_ == active.
    local_state_->SetBoolean(prefs::kPowerBatterySaver, active);
  }
  if (active != PowerStatus::Get()->IsBatterySaverActive()) {
    power_manager::SetBatterySaverModeStateRequest request;
    request.set_enabled(active);
    chromeos::PowerManagerClient::Get()->SetBatterySaverModeState(request);
  }

  if (active && !enable_record_) {
    // An enable_record_ means that we were already active, so skip metrics if
    // it exists.
    enable_record_ = EnableRecord{base::Time::Now(), reason};
    base::UmaHistogramPercentage("Ash.BatterySaver.BatteryPercent.Enabled",
                                 static_cast<int>(battery_percent));
    if (time_to_empty) {
      base::UmaHistogramCustomTimes("Ash.BatterySaver.TimeToEmpty.Enabled",
                                    *time_to_empty, base::Hours(0),
                                    base::Hours(10), 100);
    }
    if (reason == UpdateReason::kSettings) {
      base::UmaHistogramPercentage(
          "Ash.BatterySaver.BatteryPercent.EnabledSettings",
          static_cast<int>(battery_percent));
      if (time_to_empty) {
        base::UmaHistogramCustomTimes(
            "Ash.BatterySaver.TimeToEmpty.EnabledSettings", *time_to_empty,
            base::Hours(0), base::Hours(10), 100);
      }
    }
  }

  if (!active && enable_record_) {
    // NB: We show the toast after checking enable_record_ to make sure we were
    // enabled before this Disable call.
    DisplayBatterySaverModeDisabledToast();

    // Log metrics.
    base::UmaHistogramPercentage("Ash.BatterySaver.BatteryPercent.Disabled",
                                 static_cast<int>(battery_percent));
    if (time_to_empty) {
      base::UmaHistogramCustomTimes("Ash.BatterySaver.TimeToEmpty.Disabled",
                                    *time_to_empty, base::Hours(0),
                                    base::Hours(10), 100);
    }
    auto duration = base::Time::Now() - enable_record_->time;
    base::UmaHistogramCustomTimes("Ash.BatterySaver.Duration", duration,
                                  base::Hours(0), base::Hours(10), 100);
    // Duration by enabled reason metrics
    switch (enable_record_->reason) {
      case UpdateReason::kAlwaysOn:
      case UpdateReason::kCharging:
      case UpdateReason::kPowerManager:
        break;

      case UpdateReason::kLowPower:
      case UpdateReason::kThreshold:
        base::UmaHistogramLongTimes(
            "Ash.BatterySaver.Duration.EnabledNotification", duration);
        break;

      case UpdateReason::kSettings:
        base::UmaHistogramLongTimes("Ash.BatterySaver.Duration.EnabledSettings",
                                    duration);
        break;
    }
    enable_record_ = absl::nullopt;

    // Disabled reason metrics.
    switch (reason) {
      case UpdateReason::kAlwaysOn:
      case UpdateReason::kPowerManager:
        break;

      case UpdateReason::kCharging:
        base::UmaHistogramLongTimes(
            "Ash.BatterySaver.Duration.DisabledCharging", duration);
        break;

      case UpdateReason::kLowPower:
      case UpdateReason::kThreshold:
        base::UmaHistogramLongTimes(
            "Ash.BatterySaver.Duration.DisabledNotification", duration);
        break;

      case UpdateReason::kSettings:
        base::UmaHistogramLongTimes(
            "Ash.BatterySaver.Duration.DisabledSettings", duration);
        base::UmaHistogramPercentage(
            "Ash.BatterySaver.BatteryPercent.DisabledSettings",
            static_cast<int>(battery_percent));
        if (time_to_empty) {
          base::UmaHistogramCustomTimes(
              "Ash.BatterySaver.TimeToEmpty.DisabledSettings", *time_to_empty,
              base::Hours(0), base::Hours(10), 100);
        }
        break;
    }
  }
}

absl::optional<int> BatterySaverController::GetRemainingMinutes(
    const PowerStatus* status) {
  if (status->IsBatteryTimeBeingCalculated()) {
    return absl::nullopt;
  }

  const absl::optional<base::TimeDelta> remaining_time =
      status->GetBatteryTimeToEmpty();

  // Check that powerd actually provided an estimate. It doesn't if the battery
  // current is so close to zero that the estimate would be huge.
  if (!remaining_time) {
    return absl::nullopt;
  }

  return base::ClampRound(*remaining_time / base::Minutes(1));
}

}  // namespace ash
