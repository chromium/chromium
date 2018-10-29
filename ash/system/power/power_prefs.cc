// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_prefs.h"

#include <string>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/time/default_tick_clock.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

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
  ash::SessionController* controller = Shell::Get()->session_controller();
  PrefService* prefs = controller->GetPrimaryUserPrefService();
  return prefs ? prefs : controller->GetActivePrefService();
}

// Registers power prefs whose default values are the same in user prefs and
// signin prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test) {
  registry->RegisterIntegerPref(prefs::kPowerAcScreenBrightnessPercent, -1,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerAcScreenDimDelayMs, 420000,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerAcScreenOffDelayMs, 450000,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerAcScreenLockDelayMs, 0,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerAcIdleWarningDelayMs, 0,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerAcIdleDelayMs, 510000,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerBatteryScreenBrightnessPercent, -1,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerBatteryScreenDimDelayMs, 300000,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerBatteryScreenOffDelayMs, 330000,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerBatteryScreenLockDelayMs, 0,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerBatteryIdleWarningDelayMs, 0,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerBatteryIdleDelayMs, 390000,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerLockScreenDimDelayMs, 30000,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerLockScreenOffDelayMs, 40000,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerAcIdleAction,
                                chromeos::PowerPolicyController::ACTION_SUSPEND,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(prefs::kPowerUseAudioActivity, true,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(prefs::kPowerUseVideoActivity, true,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(prefs::kPowerAllowWakeLocks, true,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(prefs::kPowerAllowScreenWakeLocks, true,
                                PrefRegistry::PUBLIC);
  registry->RegisterDoublePref(prefs::kPowerPresentationScreenDimDelayFactor,
                               2.0, PrefRegistry::PUBLIC);
  registry->RegisterDoublePref(prefs::kPowerUserActivityScreenDimDelayFactor,
                               2.0, PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(prefs::kPowerWaitForInitialUserActivity, false,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(
      prefs::kPowerForceNonzeroBrightnessForUserActivity, true,
      PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(prefs::kPowerSmartDimEnabled, true,
                                PrefRegistry::PUBLIC);

  if (for_test) {
    registry->RegisterBooleanPref(prefs::kAllowScreenLock, true,
                                  PrefRegistry::PUBLIC);
    registry->RegisterBooleanPref(
        prefs::kEnableAutoScreenLock, false,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF | PrefRegistry::PUBLIC);
  } else {
    registry->RegisterForeignPref(prefs::kAllowScreenLock);
    registry->RegisterForeignPref(prefs::kEnableAutoScreenLock);
  }
}

}  // namespace

PowerPrefs::PowerPrefs(chromeos::PowerPolicyController* power_policy_controller,
                       chromeos::PowerManagerClient* power_manager_client)
    : power_policy_controller_(power_policy_controller),
      power_manager_client_observer_(this),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  DCHECK(power_manager_client);
  DCHECK(power_policy_controller_);
  DCHECK(tick_clock_);

  power_manager_client_observer_.Add(power_manager_client);
  Shell::Get()->session_controller()->AddObserver(this);
}

PowerPrefs::~PowerPrefs() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void PowerPrefs::RegisterSigninProfilePrefs(PrefRegistrySimple* registry,
                                            bool for_test) {
  RegisterProfilePrefs(registry, for_test);

  registry->RegisterIntegerPref(
      prefs::kPowerBatteryIdleAction,
      chromeos::PowerPolicyController::ACTION_SHUT_DOWN, PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(
      prefs::kPowerLidClosedAction,
      chromeos::PowerPolicyController::ACTION_SHUT_DOWN, PrefRegistry::PUBLIC);
}

// static
void PowerPrefs::RegisterUserProfilePrefs(PrefRegistrySimple* registry,
                                          bool for_test) {
  RegisterProfilePrefs(registry, for_test);

  registry->RegisterIntegerPref(prefs::kPowerBatteryIdleAction,
                                chromeos::PowerPolicyController::ACTION_SUSPEND,
                                PrefRegistry::PUBLIC);
  registry->RegisterIntegerPref(prefs::kPowerLidClosedAction,
                                chromeos::PowerPolicyController::ACTION_SUSPEND,
                                PrefRegistry::PUBLIC);
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

void PowerPrefs::UpdatePowerPolicyFromPrefs() {
  PrefService* prefs = GetPrefService();
  DCHECK(prefs);

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
  values.presentation_screen_dim_delay_factor =
      prefs->GetDouble(prefs::kPowerPresentationScreenDimDelayFactor);
  values.user_activity_screen_dim_delay_factor =
      prefs->GetDouble(prefs::kPowerUserActivityScreenDimDelayFactor);
  values.wait_for_initial_user_activity =
      prefs->GetBoolean(prefs::kPowerWaitForInitialUserActivity);
  values.force_nonzero_brightness_for_user_activity =
      prefs->GetBoolean(prefs::kPowerForceNonzeroBrightnessForUserActivity);
  values.smart_dim_enabled = prefs->GetBoolean(prefs::kPowerSmartDimEnabled);

  power_policy_controller_->ApplyPrefs(values);
}

void PowerPrefs::ObservePrefs(PrefService* prefs) {
  // Observe pref updates from locked state change and policy.
  base::RepeatingClosure update_callback(base::BindRepeating(
      &PowerPrefs::UpdatePowerPolicyFromPrefs, base::Unretained(this)));

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(prefs::kPowerAcScreenBrightnessPercent,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerAcScreenDimDelayMs, update_callback);
  pref_change_registrar_->Add(prefs::kPowerAcScreenOffDelayMs, update_callback);
  pref_change_registrar_->Add(prefs::kPowerAcScreenLockDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerAcIdleWarningDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerAcIdleDelayMs, update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryScreenBrightnessPercent,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryScreenDimDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryScreenOffDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryScreenLockDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryIdleWarningDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryIdleDelayMs, update_callback);
  pref_change_registrar_->Add(prefs::kPowerLockScreenDimDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerLockScreenOffDelayMs,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerAcIdleAction, update_callback);
  pref_change_registrar_->Add(prefs::kPowerBatteryIdleAction, update_callback);
  pref_change_registrar_->Add(prefs::kPowerLidClosedAction, update_callback);
  pref_change_registrar_->Add(prefs::kPowerUseAudioActivity, update_callback);
  pref_change_registrar_->Add(prefs::kPowerUseVideoActivity, update_callback);
  pref_change_registrar_->Add(prefs::kPowerAllowWakeLocks, update_callback);
  pref_change_registrar_->Add(prefs::kPowerAllowScreenWakeLocks,
                              update_callback);
  pref_change_registrar_->Add(prefs::kEnableAutoScreenLock, update_callback);
  pref_change_registrar_->Add(prefs::kPowerPresentationScreenDimDelayFactor,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerUserActivityScreenDimDelayFactor,
                              update_callback);
  pref_change_registrar_->Add(prefs::kPowerWaitForInitialUserActivity,
                              update_callback);
  pref_change_registrar_->Add(
      prefs::kPowerForceNonzeroBrightnessForUserActivity, update_callback);
  pref_change_registrar_->Add(prefs::kAllowScreenLock, update_callback);
  pref_change_registrar_->Add(prefs::kPowerSmartDimEnabled, update_callback);

  UpdatePowerPolicyFromPrefs();
}

}  // namespace ash
