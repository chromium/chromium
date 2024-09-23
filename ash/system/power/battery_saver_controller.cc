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
  // In some tests, we may not have ash:Shell.
  if (!ash::Shell::HasInstance()) {
    CHECK_IS_TEST();
    return nullptr;
  }
  // NB: BatterySaverController and PowerNotificationController depend on each
  // other. Depending on the order of signals received at startup, the
  // dependency could be one way or the other. To break the cycle, we return
  // nullptr here, and rely on BatterySaverController to handle
  // PowerNotificationController not being up yet.
  auto* system = ash::Shell::Get()->system_notification_controller();
  if (!system) {
    return nullptr;
  }
  auto* power = system->power_notification_controller();
  if (!power) {
    return nullptr;
  }
  return power;
}

void SetUserOptStatus(bool status) {
  if (GetPowerNotificationController()) {
    GetPowerNotificationController()->SetUserOptStatus(status);
  }
}

// Overrides the result of IsBatterySaverAllowed for testing.
std::optional<bool> override_allowed_for_testing;

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

void OverrideIsBatterySaverAllowedForTesting(std::optional<bool> isAllowed) {
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

  if (local_state_) {
    pref_change_registrar_.Init(local_state);
    pref_change_registrar_.Add(
        prefs::kPowerBatterySaver,
        base::BindRepeating(&BatterySaverController::OnSettingsPrefChanged,
                            weak_ptr_factory_.GetWeakPtr()));
    // Restore state from the saved preference value.
    OnSettingsPrefChanged();
  } else {
    CHECK_IS_TEST();
  }
}

BatterySaverController::~BatterySaverController() = default;

// static
void BatterySaverController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kPowerBatterySaver, false);

  // kPowerBatterySaverPercent is used to detect charging when the device is off
  // or sleeping. We don't get PowerStatus updates telling us a charger is
  // attached. Instead we save the battery charge percent when we do get power
  // status updates (and battery saver is enabled), and look for a jump in the
  // charge percent.
  registry->RegisterIntegerPref(prefs::kPowerBatterySaverPercent, -1);
}

// static
void BatterySaverController::ResetState(PrefService* local_state) {
  if (local_state) {
    local_state->ClearPref(prefs::kPowerBatterySaver);
    local_state->ClearPref(prefs::kPowerBatterySaverPercent);
  } else {
    CHECK_IS_TEST();
  }

  power_manager::SetBatterySaverModeStateRequest request;
  request.set_enabled(false);
  chromeos::PowerManagerClient::Get()->SetBatterySaverModeState(request);
}

void BatterySaverController::OnPowerStatusChanged() {
  if (always_on_) {
    SetState(/*active=*/true, UpdateReason::kAlwaysOn);
    return;
  }

  const auto* power_status = PowerStatus::Get();
  CHECK(power_status);
  const bool active = power_status->IsBatterySaverActive();
  const bool on_AC_power = power_status->IsMainsChargerConnected();
  const int battery_percent = power_status->GetRoundedBatteryPercent();

  // The preference is the source of truth for battery saver state. If we see
  // Power Manager disagree, update its state and return.
  // NB: This is important because Power Manager sends a PowerStatus signal as
  // part of enabling Battery Saver, but before the Battery Saver signal, so we
  // always get a spurious PowerStatus with Battery Saver disabled right after
  // enabling Battery Saver.
  if (local_state_) {
    const bool pref_active =
        local_state_->GetBoolean(prefs::kPowerBatterySaver);
    if (pref_active != active) {
      SetState(pref_active, UpdateReason::kPowerManager);
      return;
    }
  }

  // Detect charging while powered off or sleeping.
  // NB: This is above the on_AC_power check below so that we don't show the
  // disabled toast just after startup if we had Battery Saver on before
  // shutdown but are now charging. In that situation, we will restore battery
  // saver in the ctor, but then disable it in the first OnPowerStatusChanged.
  if (local_state_ && active) {
    const int pref_battery_percent =
        local_state_->GetInteger(prefs::kPowerBatterySaverPercent);
    const bool pref_battery_percent_set = pref_battery_percent >= 0;
    const bool above_activation_threshold =
        battery_percent > activation_charge_percent_;
    const bool battery_increased =
        battery_percent >=
        pref_battery_percent + kBatterySaverSleepChargeThreshold;
    if (pref_battery_percent_set && above_activation_threshold &&
        battery_increased) {
      SetState(/*active=*/false, UpdateReason::kChargeIncrease);
      return;
    } else if (battery_percent != pref_battery_percent) {
      local_state_->SetInteger(prefs::kPowerBatterySaverPercent,
                               battery_percent);
    }
  }

  // Should we turn off battery saver?
  if (active && on_AC_power) {
    SetState(/*active=*/false, UpdateReason::kCharging);
    return;
  }
}

void BatterySaverController::OnSettingsPrefChanged() {
  CHECK(local_state_);

  if (always_on_) {
    SetState(/*active=*/true, UpdateReason::kAlwaysOn);
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

void BatterySaverController::StopObservingPowerStatusForTest() {
  power_status_observation_.Reset();
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
  CHECK(power_status);

  std::optional<base::TimeDelta> time_to_empty =
      power_status->GetBatteryTimeToEmpty();
  double battery_percent = power_status->GetBatteryPercent();

  if (active && !enable_record_) {
    // An enable_record_ means that we were already active, so skip metrics if
    // it exists.
    enable_record_ = EnableRecord{base::TimeTicks::Now(), reason};
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
    // NB: We show the toast after checking enable_record_ to make sure we
    // were enabled before this Disable call.
    if (reason != UpdateReason::kSettings &&
        reason != UpdateReason::kChargeIncrease) {
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
    auto duration = base::TimeTicks::Now() - enable_record_->time;
    base::UmaHistogramCustomTimes("Ash.BatterySaver.Duration", duration,
                                  base::Hours(0), base::Hours(10), 100);
    // Duration by enabled reason metrics
    switch (enable_record_->reason) {
      case UpdateReason::kAlwaysOn:
      case UpdateReason::kCharging:
      case UpdateReason::kChargeIncrease:
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
      case UpdateReason::kChargeIncrease:
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

  const auto* power_notification_controller = GetPowerNotificationController();
  if (power_notification_controller) {
    const bool crossed_threshold =
        power_status->GetRoundedBatteryPercent() <=
        power_notification_controller->GetLowPowerPercentage();

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
  if (local_state_ &&
      active != local_state_->GetBoolean(prefs::kPowerBatterySaver)) {
    // Note: Prevents call from being re-entrant, and also allows us to
    // differentiate between the system changing this perf, vs. the user doing
    // it (e.g. from somewhere else like Settings).
    base::AutoReset<bool> in_set_state(&in_set_state_, true);
    local_state_->SetBoolean(prefs::kPowerBatterySaver, active);
  }

  if (active != power_status->IsBatterySaverActive()) {
    auto* power_manager_client = chromeos::PowerManagerClient::Get();
    CHECK(power_manager_client);
    power_manager::SetBatterySaverModeStateRequest request;
    request.set_enabled(active);
    power_manager_client->SetBatterySaverModeState(request);

    // Battery Saver percent is only tracked when battery saver is on.
    // NB: On initialization PowerStatus updates often arrive with battery saver
    // inactive after the call to SetState restoring battery saver state from
    // the pref. To preserve kPowerBatterySaverPercent, we only clear it if
    // battery saver transitions from active to inactive, not every time we see
    // a PowerStatus with it inactive.
    if (local_state_ && !active) {
      local_state_->ClearPref(prefs::kPowerBatterySaverPercent);
    }
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
  if (!local_state_) {
    return false;
  }

  // Pref is managed and set to false.
  return local_state_->IsManagedPreference(prefs::kPowerBatterySaver) &&
         !local_state_->GetBoolean(prefs::kPowerBatterySaver);
}

}  // namespace ash
