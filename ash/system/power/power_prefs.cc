// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_prefs.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/human_presence/human_presence_metrics.h"
#include "ash/system/human_presence/lock_on_leave_controller.h"
#include "ash/system/power/adaptive_charging_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_tick_clock.h"
#include "chromeos/ash/components/human_presence/human_presence_configuration.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

using PeakShiftDayConfig =
    power_manager::PowerManagementPolicy::PeakShiftDayConfig;

using AdvancedBatteryChargeModeDayConfig =
    power_manager::PowerManagementPolicy::AdvancedBatteryChargeModeDayConfig;

namespace qd_metrics = ash::quick_dim_metrics;

chromeos::PowerPolicyController::Action GetPowerPolicyAction(
    const PrefService* prefs,
    const std::string& pref_name) {
  const chromeos::PowerPolicyController::Action pref_action =
      static_cast<chromeos::PowerPolicyController::Action>(
          prefs->GetInteger(pref_name));

  // Transform the power policy action when the lock screen is disabled and
  // power preferences request to lock the screen: the session stop should be
  // requested instead.
  //
  // This resolves potential privacy issues when the device could suspend
  // before the session stop is fully finished and the login screen is shown.
  //
  // Note that the power policy prefs related to showing the lock screen on idle
  // don't have to be adjusted accordingly, as Chrome itself will perform
  // session stop instead of screen lock when the latter one is not available.
  if (pref_action == chromeos::PowerPolicyController::ACTION_SUSPEND &&
      prefs->GetBoolean(prefs::kEnableAutoScreenLock) &&
      !prefs->GetBoolean(prefs::kAllowScreenLock)) {
    return chromeos::PowerPolicyController::ACTION_STOP_SESSION;
  }

  return pref_action;
}

// Returns the PrefService that should be used for determining power-related
// behavior. When one or more users are logged in, the primary user's prefs are
// used: if more-restrictive power-related prefs are set by policy, it's most
// likely to be on this profile.
PrefService* GetPrefService() {
  ash::SessionControllerImpl* controller = Shell::Get()->session_controller();
  PrefService* prefs = controller->GetPrimaryUserPrefService();
  return prefs ? prefs : controller->GetActivePrefService();
}

// Registers power prefs whose default values are the same in user prefs and
// signin prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kPowerAcScreenBrightnessPercent, -1);
  registry->RegisterIntegerPref(prefs::kPowerAcScreenDimDelayMs, 420000);
  registry->RegisterIntegerPref(prefs::kPowerAcScreenOffDelayMs, 450000);
  registry->RegisterIntegerPref(prefs::kPowerAcScreenLockDelayMs, 0);
  registry->RegisterIntegerPref(prefs::kPowerAcIdleWarningDelayMs, 0);
  registry->RegisterIntegerPref(prefs::kPowerAcIdleDelayMs, 510000);
  registry->RegisterBooleanPref(
      prefs::kPowerAdaptiveChargingEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kPowerAdaptiveChargingNudgeShown, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(prefs::kPowerBatteryScreenBrightnessPercent,
                                -1);
  registry->RegisterIntegerPref(prefs::kPowerBatteryScreenDimDelayMs, 300000);
  registry->RegisterIntegerPref(prefs::kPowerBatteryScreenOffDelayMs, 330000);
  registry->RegisterIntegerPref(prefs::kPowerBatteryScreenLockDelayMs, 0);
  registry->RegisterIntegerPref(prefs::kPowerBatteryIdleWarningDelayMs, 0);
  registry->RegisterIntegerPref(prefs::kPowerBatteryIdleDelayMs, 390000);
  registry->RegisterIntegerPref(prefs::kPowerLockScreenDimDelayMs, 30000);
  registry->RegisterIntegerPref(prefs::kPowerLockScreenOffDelayMs, 40000);
  registry->RegisterIntegerPref(
      prefs::kPowerAcIdleAction,
      chromeos::PowerPolicyController::ACTION_SUSPEND);
  registry->RegisterBooleanPref(prefs::kPowerUseAudioActivity, true);
  registry->RegisterBooleanPref(prefs::kPowerUseVideoActivity, true);
  registry->RegisterBooleanPref(prefs::kPowerAllowWakeLocks, true);
  registry->RegisterBooleanPref(prefs::kPowerAllowScreenWakeLocks, true);
  registry->RegisterDoublePref(prefs::kPowerPresentationScreenDimDelayFactor,
                               2.0);
  registry->RegisterDoublePref(prefs::kPowerUserActivityScreenDimDelayFactor,
                               2.0);
  registry->RegisterBooleanPref(prefs::kPowerWaitForInitialUserActivity, false);
  registry->RegisterBooleanPref(
      prefs::kPowerForceNonzeroBrightnessForUserActivity, true);
  registry->RegisterBooleanPref(prefs::kPowerFastSuspendWhenBacklightsForcedOff,
                                true);
  registry->RegisterBooleanPref(prefs::kPowerSmartDimEnabled, true);
  registry->RegisterBooleanPref(prefs::kPowerAlsLoggingEnabled, false);
  registry->RegisterBooleanPref(prefs::kPowerQuickDimEnabled, false);
  registry->RegisterIntegerPref(prefs::kPowerQuickLockDelay,
                                hps::GetQuickLockDelay().InMilliseconds());

  registry->RegisterBooleanPref(prefs::kAllowScreenLock, true);
  registry->RegisterBooleanPref(
      prefs::kEnableAutoScreenLock, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void UpdateAdaptiveChargingConfigsFromFinch(
    chromeos::PowerPolicyController::PrefValues* values) {
  // Default values of the settings.
  constexpr double kDefaultAdaptiveChargingMinProbability = 0.35;
  constexpr int kDefaultAdaptiveChargingHoldPercent = 80;
  constexpr double kDefaultAdaptiveChargingMaxDelayPercentile = 0.3;
  constexpr int kDefaultAdaptiveChargingMinDaysHistory = 14;
  constexpr double kDefaultAdaptiveChargingMinFullOnAcRatio = 0.5;

  // An AdaptiveCharging decision is considered to be reliable if the inference
  // score is higher than this number.
  values->adaptive_charging_min_probability =
      base::GetFieldTrialParamByFeatureAsDouble(
          ash::features::kAdaptiveCharging, "adaptive_charging_min_probability",
          kDefaultAdaptiveChargingMinProbability);

  // The AdaptiveCharging will delay the charging when the battery level is at
  // or higher than this number until AdaptiveCharging is over.
  values->adaptive_charging_hold_percent =
      base::GetFieldTrialParamByFeatureAsInt(
          ash::features::kAdaptiveCharging, "adaptive_charging_hold_percent",
          kDefaultAdaptiveChargingHoldPercent);

  // The max delay that AdaptiveCharging applies to hold the charging is capped
  // by this percentile of the device's charge history durations.
  values->adaptive_charging_max_delay_percentile =
      base::GetFieldTrialParamByFeatureAsDouble(
          ash::features::kAdaptiveCharging,
          "adaptive_charging_max_delay_percentile",
          kDefaultAdaptiveChargingMaxDelayPercentile);

  // If charging history doesn't contain at least this amount of days,
  // AdaptiveCharging is disabled.
  values->adaptive_charging_min_days_history =
      base::GetFieldTrialParamByFeatureAsInt(
          ash::features::kAdaptiveCharging,
          "adaptive_charging_min_days_history",
          kDefaultAdaptiveChargingMinDaysHistory);

  // If charging history doesn't have full_on_ac_ratio >= this min value,
  // AdaptiveCharging is disabled.
  values->adaptive_charging_min_full_on_ac_ratio =
      base::GetFieldTrialParamByFeatureAsDouble(
          ash::features::kAdaptiveCharging,
          "adaptive_charging_min_full_on_ac_ratio",
          kDefaultAdaptiveChargingMinFullOnAcRatio);
}

}  // namespace

PowerPrefs::PowerPrefs(chromeos::PowerPolicyController* power_policy_controller,
                       chromeos::PowerManagerClient* power_manager_client,
                       PrefService* local_state)
    : power_policy_controller_(power_policy_controller),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      local_state_(local_state) {
  DCHECK(power_manager_client);
  DCHECK(power_policy_controller_);
  DCHECK(tick_clock_);

  // Only construct lock_on_leave_controller_ if quick dim is enabled.
  if (features::IsQuickDimEnabled())
    lock_on_leave_controller_ = std::make_unique<LockOnLeaveController>();

  power_manager_client_observation_.Observe(power_manager_client);
  Shell::Get()->session_controller()->AddObserver(this);

  // |local_state_| could be null in tests.
  if (local_state_)
    ObserveLocalStatePrefs(local_state_);
}

PowerPrefs::~PowerPrefs() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void PowerPrefs::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kPowerPeakShiftEnabled, false);
  registry->RegisterIntegerPref(prefs::kPowerPeakShiftBatteryThreshold, -1);
  registry->RegisterDictionaryPref(prefs::kPowerPeakShiftDayConfig);

  registry->RegisterBooleanPref(prefs::kBootOnAcEnabled, false);

  registry->RegisterBooleanPref(prefs::kAdvancedBatteryChargeModeEnabled,
                                false);
  registry->RegisterDictionaryPref(prefs::kAdvancedBatteryChargeModeDayConfig);

  registry->RegisterIntegerPref(prefs::kBatteryChargeMode, -1);
  registry->RegisterIntegerPref(prefs::kBatteryChargeCustomStartCharging, -1);
  registry->RegisterIntegerPref(prefs::kBatteryChargeCustomStopCharging, -1);

  registry->RegisterBooleanPref(prefs::kUsbPowerShareEnabled, true);
}

// static
void PowerPrefs::RegisterSigninProfilePrefs(PrefRegistrySimple* registry) {
  RegisterProfilePrefs(registry);

  registry->RegisterIntegerPref(
      prefs::kPowerBatteryIdleAction,
      chromeos::PowerPolicyController::ACTION_SHUT_DOWN);
  registry->RegisterIntegerPref(
      prefs::kPowerLidClosedAction,
      chromeos::PowerPolicyController::ACTION_SHUT_DOWN);
}

// static
void PowerPrefs::RegisterUserProfilePrefs(PrefRegistrySimple* registry) {
  RegisterProfilePrefs(registry);

  registry->RegisterIntegerPref(
      prefs::kPowerBatteryIdleAction,
      chromeos::PowerPolicyController::ACTION_SUSPEND);
  registry->RegisterIntegerPref(
      prefs::kPowerLidClosedAction,
      chromeos::PowerPolicyController::ACTION_SUSPEND);
}

void PowerPrefs::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  const bool already_off = !screen_idle_off_time_.is_null();
  if (proto.off() == already_off)
    return;

  screen_idle_off_time_ =
      proto.off() ? tick_clock_->NowTicks() : base::TimeTicks();

  // If the screen is locked and we're no longer idle, we may need to switch to
  // the lock-based delays.
  if (!screen_lock_time_.is_null() && !proto.off())
    UpdatePowerPolicyFromPrefs();
}

void PowerPrefs::OnLockStateChanged(bool locked) {
  const bool already_locked = !screen_lock_time_.is_null();
  if (locked == already_locked)
    return;

  screen_lock_time_ = locked ? tick_clock_->NowTicks() : base::TimeTicks();
  // OnLockStateChanged could be called before ash connects user prefs in tests.
  if (GetPrefService())
    UpdatePowerPolicyFromPrefs();
}

void PowerPrefs::OnSigninScreenPrefServiceInitialized(PrefService* prefs) {
  ObservePrefs(prefs);
}

void PowerPrefs::OnActiveUserPrefServiceChanged(PrefService* prefs) {
  ObservePrefs(prefs);
}

void PowerPrefs::UpdatePowerPolicyFromPrefsChange() {
  PrefService* prefs = GetPrefService();
  if (!prefs)
    return;

  bool new_quick_dim_pref_enabled =
      prefs->GetBoolean(prefs::kPowerQuickDimEnabled);
  if (quick_dim_pref_enabled_ != new_quick_dim_pref_enabled) {
    quick_dim_pref_enabled_ = new_quick_dim_pref_enabled;
    base::UmaHistogramBoolean(qd_metrics::kEnabledHistogramName,
                              quick_dim_pref_enabled_);
  }

  UpdatePowerPolicyFromPrefs();
}

void PowerPrefs::UpdatePowerPolicyFromPrefs() {
  PrefService* prefs = GetPrefService();
  if (!prefs || !local_state_)
    return;

  // It's possible to end up in a situation where a shortened lock-screen idle
  // delay would cause the system to suspend immediately as soon as the screen
  // is locked due to inactivity; see https://crbug.com/807861 for the gory
  // details. To avoid this, don't switch to the shorter delays immediately when
  // the screen is locked automatically (as indicated by the screen having been
  // previously turned off for inactivity).
  bool use_lock_delays = !screen_lock_time_.is_null() &&
                         (screen_idle_off_time_.is_null() ||
                          screen_idle_off_time_ > screen_lock_time_);

  chromeos::PowerPolicyController::PrefValues values;

  values.ac_brightness_percent =
      prefs->GetInteger(prefs::kPowerAcScreenBrightnessPercent);
  values.ac_screen_dim_delay_ms =
      prefs->GetInteger(use_lock_delays ? prefs::kPowerLockScreenDimDelayMs
                                        : prefs::kPowerAcScreenDimDelayMs);
  values.ac_screen_off_delay_ms =
      prefs->GetInteger(use_lock_delays ? prefs::kPowerLockScreenOffDelayMs
                                        : prefs::kPowerAcScreenOffDelayMs);
  values.ac_screen_lock_delay_ms =
      prefs->GetInteger(prefs::kPowerAcScreenLockDelayMs);
  values.ac_idle_warning_delay_ms =
      prefs->GetInteger(prefs::kPowerAcIdleWarningDelayMs);
  values.ac_idle_delay_ms = prefs->GetInteger(prefs::kPowerAcIdleDelayMs);
  values.battery_brightness_percent =
      prefs->GetInteger(prefs::kPowerBatteryScreenBrightnessPercent);
  values.battery_screen_dim_delay_ms =
      prefs->GetInteger(use_lock_delays ? prefs::kPowerLockScreenDimDelayMs
                                        : prefs::kPowerBatteryScreenDimDelayMs);
  values.battery_screen_off_delay_ms =
      prefs->GetInteger(use_lock_delays ? prefs::kPowerLockScreenOffDelayMs
                                        : prefs::kPowerBatteryScreenOffDelayMs);
  values.battery_screen_lock_delay_ms =
      prefs->GetInteger(prefs::kPowerBatteryScreenLockDelayMs);
  values.battery_idle_warning_delay_ms =
      prefs->GetInteger(prefs::kPowerBatteryIdleWarningDelayMs);
  values.battery_idle_delay_ms =
      prefs->GetInteger(prefs::kPowerBatteryIdleDelayMs);
  values.ac_idle_action =
      GetPowerPolicyAction(prefs, prefs::kPowerAcIdleAction);
  values.battery_idle_action =
      GetPowerPolicyAction(prefs, prefs::kPowerBatteryIdleAction);
  values.lid_closed_action =
      GetPowerPolicyAction(prefs, prefs::kPowerLidClosedAction);
  values.use_audio_activity = prefs->GetBoolean(prefs::kPowerUseAudioActivity);
  values.use_video_activity = prefs->GetBoolean(prefs::kPowerUseVideoActivity);
  values.allow_wake_locks = prefs->GetBoolean(prefs::kPowerAllowWakeLocks);
  values.allow_screen_wake_locks =
      prefs->GetBoolean(prefs::kPowerAllowScreenWakeLocks);
  values.enable_auto_screen_lock =
      prefs->GetBoolean(prefs::kEnableAutoScreenLock);

  // Screen-dim deferral in response to user activity predictions can interact
  // poorly with delay scaling, resulting in the system staying awake for a long
  // time if a prediction is wrong. https://crbug.com/888392.
  if (prefs->GetBoolean(prefs::kPowerSmartDimEnabled) &&
      base::FeatureList::IsEnabled(features::kUserActivityPrediction)) {
    values.presentation_screen_dim_delay_factor = 1.0;
    values.user_activity_screen_dim_delay_factor = 1.0;
  } else {
    values.presentation_screen_dim_delay_factor =
        prefs->GetDouble(prefs::kPowerPresentationScreenDimDelayFactor);
    values.user_activity_screen_dim_delay_factor =
        prefs->GetDouble(prefs::kPowerUserActivityScreenDimDelayFactor);
  }

  // Only set power_manager and lock-on-leave if quick dim is enabled.
  if (lock_on_leave_controller_) {
    if (prefs->GetBoolean(prefs::kPowerQuickDimEnabled)) {
      values.battery_quick_dim_delay_ms =
          hps::GetQuickDimDelay().InMilliseconds();
      values.ac_quick_dim_delay_ms = hps::GetQuickDimDelay().InMilliseconds();

      values.battery_quick_lock_delay_ms =
          prefs->GetInteger(prefs::kPowerQuickLockDelay);
      values.ac_quick_lock_delay_ms =
          prefs->GetInteger(prefs::kPowerQuickLockDelay);

      values.send_feedback_if_undimmed = hps::GetQuickDimFeedbackEnabled();

      lock_on_leave_controller_->EnableLockOnLeave();
    } else {
      lock_on_leave_controller_->DisableLockOnLeave();
    }
  }

  values.wait_for_initial_user_activity =
      prefs->GetBoolean(prefs::kPowerWaitForInitialUserActivity);
  values.force_nonzero_brightness_for_user_activity =
      prefs->GetBoolean(prefs::kPowerForceNonzeroBrightnessForUserActivity);
  values.fast_suspend_when_backlights_forced_off =
      prefs->GetBoolean(prefs::kPowerFastSuspendWhenBacklightsForcedOff);

  if (local_state_->GetBoolean(prefs::kPowerPeakShiftEnabled) &&
      local_state_->IsManagedPreference(prefs::kPowerPeakShiftEnabled) &&
      local_state_->IsManagedPreference(
          prefs::kPowerPeakShiftBatteryThreshold) &&
      local_state_->IsManagedPreference(prefs::kPowerPeakShiftDayConfig)) {
    const base::Value::Dict& configs_value =
        local_state_->GetDict(prefs::kPowerPeakShiftDayConfig);
    std::vector<PeakShiftDayConfig> configs;
    if (chromeos::PowerPolicyController::GetPeakShiftDayConfigs(configs_value,
                                                                &configs)) {
      values.peak_shift_enabled = true;
      values.peak_shift_battery_threshold =
          local_state_->GetInteger(prefs::kPowerPeakShiftBatteryThreshold);
      values.peak_shift_day_configs = std::move(configs);
    } else {
      LOG(WARNING) << "Invalid Peak Shift day configs format: "
                   << configs_value;
    }
  }

  if (local_state_->GetBoolean(prefs::kAdvancedBatteryChargeModeEnabled) &&
      local_state_->IsManagedPreference(
          prefs::kAdvancedBatteryChargeModeEnabled) &&
      local_state_->IsManagedPreference(
          prefs::kAdvancedBatteryChargeModeDayConfig)) {
    const base::Value::Dict& configs_value =
        local_state_->GetDict(prefs::kAdvancedBatteryChargeModeDayConfig);
    std::vector<AdvancedBatteryChargeModeDayConfig> configs;
    if (chromeos::PowerPolicyController::GetAdvancedBatteryChargeModeDayConfigs(
            configs_value, &configs)) {
      values.advanced_battery_charge_mode_enabled = true;
      values.advanced_battery_charge_mode_day_configs = std::move(configs);
    } else {
      LOG(WARNING)
          << "Invalid Advanced Battery Charge Mode day configs format: "
          << configs_value;
    }
  }

  if (local_state_->IsManagedPreference(prefs::kBatteryChargeMode)) {
    if (chromeos::PowerPolicyController::GetBatteryChargeModeFromInteger(
            local_state_->GetInteger(prefs::kBatteryChargeMode),
            &values.battery_charge_mode)) {
      if (local_state_->IsManagedPreference(
              prefs::kBatteryChargeCustomStartCharging) &&
          local_state_->IsManagedPreference(
              prefs::kBatteryChargeCustomStopCharging)) {
        values.custom_charge_start =
            local_state_->GetInteger(prefs::kBatteryChargeCustomStartCharging);
        values.custom_charge_stop =
            local_state_->GetInteger(prefs::kBatteryChargeCustomStopCharging);
      }
    } else {
      LOG(WARNING) << "Invalid Battery Charge Mode value: "
                   << local_state_->GetInteger(prefs::kBatteryChargeMode);
    }
  }

  if (local_state_->IsManagedPreference(prefs::kBootOnAcEnabled)) {
    values.boot_on_ac = local_state_->GetBoolean(prefs::kBootOnAcEnabled);
  }

  if (local_state_->IsManagedPreference(prefs::kUsbPowerShareEnabled)) {
    values.usb_power_share =
        local_state_->GetBoolean(prefs::kUsbPowerShareEnabled);
  }

  if (features::IsAdaptiveChargingEnabled() &&
      Shell::Get()
          ->adaptive_charging_controller()
          ->IsAdaptiveChargingSupported()) {
    values.adaptive_charging_enabled =
        prefs->GetBoolean(prefs::kPowerAdaptiveChargingEnabled);
    if (values.adaptive_charging_enabled) {
      UpdateAdaptiveChargingConfigsFromFinch(&values);
    }
  }

  power_policy_controller_->ApplyPrefs(values);
}

void PowerPrefs::ObservePrefs(PrefService* prefs) {
  // Store initial state of the quick dim preference to detect whether it has
  // been manually flipped.
  quick_dim_pref_enabled_ = prefs->GetBoolean(prefs::kPowerQuickDimEnabled);

  // Observe pref updates from policy.
  base::RepeatingClosure update_callback(base::BindRepeating(
      &PowerPrefs::UpdatePowerPolicyFromPrefsChange, base::Unretained(this)));

  profile_registrar_ = std::make_unique<PrefChangeRegistrar>();
  profile_registrar_->Init(prefs);
  profile_registrar_->Add(prefs::kPowerAcScreenBrightnessPercent,
                          update_callback);
  profile_registrar_->Add(prefs::kPowerAcScreenDimDelayMs, update_callback);
  profile_registrar_->Add(prefs::kPowerAcScreenOffDelayMs, update_callback);
  profile_registrar_->Add(prefs::kPowerAcScreenLockDelayMs, update_callback);
  profile_registrar_->Add(prefs::kPowerAcIdleWarningDelayMs, update_callback);
  profile_registrar_->Add(prefs::kPowerAcIdleDelayMs, update_callback);
  profile_registrar_->Add(prefs::kPowerBatteryScreenBrightnessPercent,
                          update_callback);
  profile_registrar_->Add(prefs::kPowerBatteryScreenDimDelayMs,
                          update_callback);
  profile_registrar_->Add(prefs::kPowerBatteryScreenOffDelayMs,
                          update_callback);
  profile_registrar_->Add(prefs::kPowerBatteryScreenLockDelayMs,
                          update_callback);
  profile_registrar_->Add(prefs::kPowerBatteryIdleWarningDelayMs,
                          update_callback);
  profile_registrar_->Add(prefs::kPowerBatteryIdleDelayMs, update_callback);
  profile_registrar_->Add(prefs::kPowerLockScreenDimDelayMs, update_callback);
  profile_registrar_->Add(prefs::kPowerLockScreenOffDelayMs, update_callback);
  profile_registrar_->Add(prefs::kPowerAcIdleAction, update_callback);
  profile_registrar_->Add(prefs::kPowerBatteryIdleAction, update_callback);
  profile_registrar_->Add(prefs::kPowerLidClosedAction, update_callback);
  profile_registrar_->Add(prefs::kPowerUseAudioActivity, update_callback);
  profile_registrar_->Add(prefs::kPowerUseVideoActivity, update_callback);
  profile_registrar_->Add(prefs::kPowerAllowWakeLocks, update_callback);
  profile_registrar_->Add(prefs::kPowerAllowScreenWakeLocks, update_callback);
  profile_registrar_->Add(prefs::kEnableAutoScreenLock, update_callback);
  profile_registrar_->Add(prefs::kPowerPresentationScreenDimDelayFactor,
                          update_callback);
  profile_registrar_->Add(prefs::kPowerUserActivityScreenDimDelayFactor,
                          update_callback);
  profile_registrar_->Add(prefs::kPowerWaitForInitialUserActivity,
                          update_callback);
  profile_registrar_->Add(prefs::kPowerForceNonzeroBrightnessForUserActivity,
                          update_callback);
  profile_registrar_->Add(prefs::kAllowScreenLock, update_callback);
  profile_registrar_->Add(prefs::kPowerSmartDimEnabled, update_callback);
  profile_registrar_->Add(prefs::kPowerFastSuspendWhenBacklightsForcedOff,
                          update_callback);
  profile_registrar_->Add(prefs::kPowerAlsLoggingEnabled, update_callback);
  profile_registrar_->Add(prefs::kPowerQuickDimEnabled, update_callback);
  profile_registrar_->Add(prefs::kPowerQuickLockDelay, update_callback);
  profile_registrar_->Add(prefs::kPowerAdaptiveChargingEnabled,
                          update_callback);

  UpdatePowerPolicyFromPrefs();
}

void PowerPrefs::ObserveLocalStatePrefs(PrefService* prefs) {
  // Observe pref updates from locked state change and policy.
  base::RepeatingClosure update_callback(base::BindRepeating(
      &PowerPrefs::UpdatePowerPolicyFromPrefs, base::Unretained(this)));

  local_state_registrar_ = std::make_unique<PrefChangeRegistrar>();
  local_state_registrar_->Init(prefs);
  local_state_registrar_->Add(prefs::kPowerPeakShiftEnabled, update_callback);
  local_state_registrar_->Add(prefs::kPowerPeakShiftBatteryThreshold,
                              update_callback);
  local_state_registrar_->Add(prefs::kPowerPeakShiftDayConfig, update_callback);

  local_state_registrar_->Add(prefs::kAdvancedBatteryChargeModeEnabled,
                              update_callback);
  local_state_registrar_->Add(prefs::kAdvancedBatteryChargeModeDayConfig,
                              update_callback);

  local_state_registrar_->Add(prefs::kBatteryChargeMode, update_callback);
  local_state_registrar_->Add(prefs::kBatteryChargeCustomStartCharging,
                              update_callback);
  local_state_registrar_->Add(prefs::kBatteryChargeCustomStopCharging,
                              update_callback);

  local_state_registrar_->Add(prefs::kBootOnAcEnabled, update_callback);

  local_state_registrar_->Add(prefs::kUsbPowerShareEnabled, update_callback);

  UpdatePowerPolicyFromPrefs();
}

}  // namespace ash
