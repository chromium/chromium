// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_saver_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/power_notification_controller.h"
#include "ash/system/system_notification_controller.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace {

ash::PowerNotificationController* GetPowerNotificationController() {
  if (ash::Shell::Get()->system_notification_controller()) {
    return ash::Shell::Get()
        ->system_notification_controller()
        ->power_notification_controller();
  }
  return nullptr;
}

void SetUserOptStatus(bool status) {
  if (GetPowerNotificationController()) {
    GetPowerNotificationController()->SetUserOptStatus(status);
  }
}

// Overrides the result of IsBatterySaverAllowed for testing.
absl::optional<bool> override_allowed_for_testing;

}  // namespace

namespace ash {

bool IsBatterySaverAllowed() {
  if (override_allowed_for_testing) {
    CHECK_IS_TEST();
    return *override_allowed_for_testing;
  }
  if (features::IsBatterySaverAvailable()) {
    return !Shell::Get()->battery_saver_controller()->IsDisabledByPolicy();
  }
  return false;
}

void OverrideIsBatterySaverAllowedForTesting(absl::optional<bool> isAllowed) {
  CHECK_IS_TEST();
  override_allowed_for_testing = isAllowed;
}

BatterySaverController::BatterySaverController(PrefService* local_state)
    : local_state_(local_state),
      activation_charge_percent_(
          features::kBatterySaverActivationChargePercent.Get()),
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

// static
void BatterySaverController::ResetState(PrefService* local_state) {
  local_state->ClearPref(prefs::kPowerBatterySaver);
  power_manager::SetBatterySaverModeStateRequest request;
  request.set_enabled(false);
  chromeos::PowerManagerClient::Get()->SetBatterySaverModeState(request);
}

void BatterySaverController::OnPowerStatusChanged() {
  if (always_on_) {
    SetState(true, UpdateReason::kAlwaysOn);
    return;
  }

  const auto* power_status = PowerStatus::Get();
  const bool active = power_status->IsBatterySaverActive();
  const bool on_AC_power = power_status->IsMainsChargerConnected();

  // The preference is the source of truth for battery saver state. If we see
  // Power Manager disagree, update its state and return.
  // NB: This is important because Power Manager sends a PowerStatus signal as
  // part of enabling Battery Saver, but before the Battery Saver signal, so we
  // always get a spurious PowerStatus with Battery Saver disabled right after
  // enabling Battery Saver.
  const bool pref_active = local_state_->GetBoolean(prefs::kPowerBatterySaver);
  if (pref_active != active) {
    SetState(pref_active, UpdateReason::kPowerManager);
    return;
  }

  // Should we turn off battery saver?
  if (active && on_AC_power) {
    SetState(false, UpdateReason::kCharging);
    return;
  }
}

void BatterySaverController::OnSettingsPrefChanged() {
  if (always_on_) {
    SetState(true, UpdateReason::kAlwaysOn);
    return;
  }

  // We can tell whenever a user issued a toggle by counting the number of
  // system issued updates. OnSettingsPrefChanged() will get called user toggled
  // + system toggled number of times. Therefore, we will end up calling
  // SetState user toggled number of times. This works since the order of the
  // requests (user vs. system) doesn't matter.
  if (in_set_state_) {
    return;
  }

  // OS Settings has changed the pref, tell Power Manager.
  SetState(local_state_->GetBoolean(prefs::kPowerBatterySaver),
           UpdateReason::kSettings);
}

void BatterySaverController::ClearBatterySaverModeToast() {
  ToastManager* toast_manager = ToastManager::Get();
  // `toast_manager` can be null when this function is called in the unit tests
  // due to initialization priority.
  if (toast_manager == nullptr) {
    return;
  }

  toast_manager->Cancel(kBatterySaverToastId);
}

void BatterySaverController::ShowBatterySaverModeToastHelper(
    const ToastCatalogName catalog_name,
    const std::u16string& toast_text) {
  ToastManager* toast_manager = ToastManager::Get();
  // `toast_manager` can be null when this function is called in the unit tests
  // due to initialization priority.
  if (toast_manager == nullptr) {
    return;
  }

  toast_manager->Cancel(kBatterySaverToastId);
  toast_manager->Show(ToastData(kBatterySaverToastId, catalog_name, toast_text,
                                ToastData::kDefaultToastDuration, true));
}

void BatterySaverController::ShowBatterySaverModeDisabledToast() {
  ShowBatterySaverModeToastHelper(
      ToastCatalogName::kBatterySaverDisabled,
      l10n_util::GetStringUTF16(IDS_ASH_BATTERY_SAVER_DISABLED_TOAST_TEXT));
}

void BatterySaverController::ShowBatterySaverModeEnabledToast() {
  ShowBatterySaverModeToastHelper(
      ToastCatalogName::kBatterySaverEnabled,
      l10n_util::GetStringUTF16(IDS_ASH_BATTERY_SAVER_ENABLED_TOAST_TEXT));
}

void BatterySaverController::SetState(bool active, UpdateReason reason) {
  auto* power_status = PowerStatus::Get();
  std::optional<base::TimeDelta> time_to_empty =
      power_status->GetBatteryTimeToEmpty();
  double battery_percent = power_status->GetBatteryPercent();

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
    if (reason != UpdateReason::kSettings) {
      ShowBatterySaverModeDisabledToast();
    }

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
    enable_record_ = std::nullopt;

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

  if (GetPowerNotificationController()) {
    const bool crossed_threshold =
        PowerStatus::Get()->GetRoundedBatteryPercent() <=
        GetPowerNotificationController()->GetLowPowerPercentage();

    // For auto-enabled, only update the user_opt_status_ when we are at or
    // below the threshold.This way, auto-enable kicks in from threshold+1% ->
    // threshold% even if the user has BSM disabled (either manually or via
    // restored local pref) beforehand.
    // If we are in the opt-in branch, we should capture user intent at any
    // threshold.
    const bool should_capture_user_intent =
        (crossed_threshold ||
         features::kBatterySaverNotificationBehavior.Get() ==
             features::kBSMOptIn);

    if (reason == UpdateReason::kSettings && should_capture_user_intent) {
      // Whether user_opt_status_ is true or false when active is true or false
      // depends on the experiment arm we are in.
      SetUserOptStatus(features::kBatterySaverNotificationBehavior.Get() ==
                               features::kBSMAutoEnable
                           ? !active
                           : active);
    }
  }

  // Update pref and Power Manager state.
  if (active != local_state_->GetBoolean(prefs::kPowerBatterySaver)) {
    // Note: Prevents call from being re-entrant, and also allows us to
    // differentiate between the system changing this perf, vs. the user doing
    // it (e.g. from somewhere else like Settings).
    base::AutoReset<bool> in_set_state(&in_set_state_, true);
    local_state_->SetBoolean(prefs::kPowerBatterySaver, active);
  }
  if (active != PowerStatus::Get()->IsBatterySaverActive()) {
    power_manager::SetBatterySaverModeStateRequest request;
    request.set_enabled(active);
    chromeos::PowerManagerClient::Get()->SetBatterySaverModeState(request);
  }
}

bool BatterySaverController::IsBatterySaverSupported() const {
  const std::optional<power_manager::PowerSupplyProperties>& proto =
      chromeos::PowerManagerClient::Get()->GetLastStatus();
  if (!proto) {
    return false;
  }
  return proto->battery_state() !=
         power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT;
}

bool BatterySaverController::IsDisabledByPolicy() const {
  // Pref is managed and set to false.
  return local_state_->IsManagedPreference(prefs::kPowerBatterySaver) &&
         !local_state_->GetBoolean(prefs::kPowerBatterySaver);
}

}  // namespace ash
