// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_prefs.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"

using session_manager::SessionState;

namespace ash {

namespace {

// Screen lock state that determines which delays are used by
// GetExpectedPowerPolicyForPrefs().
enum class ScreenLockState {
  LOCKED,
  UNLOCKED,
};

PrefService* GetSigninScreenPrefService() {
  return Shell::Get()->session_controller()->GetSigninScreenPrefService();
}

// Returns prefs for the user identified by |user_email|, or null if the user's
// prefs are unavailable (e.g. because they don't exist).
PrefService* GetUserPrefService(const std::string& user_email) {
  return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
      AccountId::FromUserEmail(user_email));
}

std::string GetExpectedPowerPolicyForPrefs(PrefService* prefs,
                                           ScreenLockState screen_lock_state) {
  const bool is_smart_dim_enabled =
      prefs->GetBoolean(prefs::kPowerSmartDimEnabled);

  power_manager::PowerManagementPolicy expected_policy;
  expected_policy.mutable_ac_delays()->set_screen_dim_ms(
      prefs->GetInteger(screen_lock_state == ScreenLockState::LOCKED
                            ? prefs::kPowerLockScreenDimDelayMs
                            : prefs::kPowerAcScreenDimDelayMs));
  expected_policy.mutable_ac_delays()->set_screen_off_ms(
      prefs->GetInteger(screen_lock_state == ScreenLockState::LOCKED
                            ? prefs::kPowerLockScreenOffDelayMs
                            : prefs::kPowerAcScreenOffDelayMs));
  expected_policy.mutable_ac_delays()->set_screen_lock_ms(
      prefs->GetInteger(prefs::kPowerAcScreenLockDelayMs));
  expected_policy.mutable_ac_delays()->set_idle_warning_ms(
      prefs->GetInteger(prefs::kPowerAcIdleWarningDelayMs));
  expected_policy.mutable_ac_delays()->set_idle_ms(
      prefs->GetInteger(prefs::kPowerAcIdleDelayMs));
  expected_policy.mutable_battery_delays()->set_screen_dim_ms(
      prefs->GetInteger(screen_lock_state == ScreenLockState::LOCKED
                            ? prefs::kPowerLockScreenDimDelayMs
                            : prefs::kPowerBatteryScreenDimDelayMs));
  expected_policy.mutable_battery_delays()->set_screen_off_ms(
      prefs->GetInteger(screen_lock_state == ScreenLockState::LOCKED
                            ? prefs::kPowerLockScreenOffDelayMs
                            : prefs::kPowerBatteryScreenOffDelayMs));
  expected_policy.mutable_battery_delays()->set_screen_lock_ms(
      prefs->GetInteger(prefs::kPowerBatteryScreenLockDelayMs));
  expected_policy.mutable_battery_delays()->set_idle_warning_ms(
      prefs->GetInteger(prefs::kPowerBatteryIdleWarningDelayMs));
  expected_policy.mutable_battery_delays()->set_idle_ms(
      prefs->GetInteger(prefs::kPowerBatteryIdleDelayMs));
  expected_policy.set_ac_idle_action(
      static_cast<power_manager::PowerManagementPolicy_Action>(
          prefs->GetInteger(prefs::kPowerAcIdleAction)));
  expected_policy.set_battery_idle_action(
      static_cast<power_manager::PowerManagementPolicy_Action>(
          prefs->GetInteger(prefs::kPowerBatteryIdleAction)));
  expected_policy.set_lid_closed_action(
      static_cast<power_manager::PowerManagementPolicy_Action>(
          prefs->GetInteger(prefs::kPowerLidClosedAction)));
  expected_policy.set_use_audio_activity(
      prefs->GetBoolean(prefs::kPowerUseAudioActivity));
  expected_policy.set_use_video_activity(
      prefs->GetBoolean(prefs::kPowerUseVideoActivity));
  if (is_smart_dim_enabled) {
    // Screen-dim scaling factors are disabled by PowerPolicyController when
    // smart-dimming is enabled.
    expected_policy.set_presentation_screen_dim_delay_factor(1.0);
    expected_policy.set_user_activity_screen_dim_delay_factor(1.0);
  } else {
    expected_policy.set_presentation_screen_dim_delay_factor(
        prefs->GetDouble(prefs::kPowerPresentationScreenDimDelayFactor));
    expected_policy.set_user_activity_screen_dim_delay_factor(
        prefs->GetDouble(prefs::kPowerUserActivityScreenDimDelayFactor));
  }
  expected_policy.set_wait_for_initial_user_activity(
      prefs->GetBoolean(prefs::kPowerWaitForInitialUserActivity));
  expected_policy.set_force_nonzero_brightness_for_user_activity(
      prefs->GetBoolean(prefs::kPowerForceNonzeroBrightnessForUserActivity));

  // Device-level prefs do not exist in the user-level |prefs|.
  expected_policy.mutable_battery_charge_mode()->set_mode(
      power_manager::PowerManagementPolicy::BatteryChargeMode::STANDARD);
  expected_policy.set_boot_on_ac(false);
  expected_policy.set_usb_power_share(true);

  expected_policy.set_reason("Prefs");
  return chromeos::PowerPolicyController::GetPolicyDebugString(expected_policy);
}

bool GetExpectedAllowScreenWakeLocksForPrefs(PrefService* prefs) {
  return prefs->GetBoolean(prefs::kPowerAllowScreenWakeLocks);
}

std::string GetExpectedPeakShiftPolicyForPrefs(PrefService* prefs) {
  DCHECK(prefs);

  std::vector<power_manager::PowerManagementPolicy::PeakShiftDayConfig> configs;
  EXPECT_TRUE(chromeos::PowerPolicyController::GetPeakShiftDayConfigs(
      *prefs->GetDictionary(prefs::kPowerPeakShiftDayConfig), &configs));

  power_manager::PowerManagementPolicy expected_policy;
  expected_policy.set_peak_shift_battery_percent_threshold(
      prefs->GetInteger(prefs::kPowerPeakShiftBatteryThreshold));
  *expected_policy.mutable_peak_shift_day_configs() = {configs.begin(),
                                                       configs.end()};

  return chromeos::PowerPolicyController::GetPeakShiftPolicyDebugString(
      expected_policy);
}

std::string GetExpectedAdvancedBatteryChargeModePolicyForPrefs(
    PrefService* prefs) {
  DCHECK(prefs);

  std::vector<
      power_manager::PowerManagementPolicy::AdvancedBatteryChargeModeDayConfig>
      configs;
  EXPECT_TRUE(
      chromeos::PowerPolicyController::GetAdvancedBatteryChargeModeDayConfigs(
          *prefs->GetDictionary(prefs::kAdvancedBatteryChargeModeDayConfig),
          &configs));

  power_manager::PowerManagementPolicy expected_policy;
  *expected_policy.mutable_advanced_battery_charge_mode_day_configs() = {
      configs.begin(), configs.end()};

  return chromeos::PowerPolicyController::
      GetAdvancedBatteryChargeModePolicyDebugString(expected_policy);
}

void DecodeJsonStringAndNormalize(const std::string& json_string,
                                  base::Value* value) {
  base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
  base::Optional<base::Value> read_value = reader.ReadToValue(json_string);
  ASSERT_EQ(reader.GetErrorMessage(), "");
  ASSERT_TRUE(read_value.has_value());
  *value = std::move(read_value.value());
}

}  // namespace

class PowerPrefsTest : public NoSessionAshTestBase {
 protected:
  PowerPrefsTest() = default;
  ~PowerPrefsTest() override = default;

  // NoSessionAshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    power_policy_controller_ = chromeos::PowerPolicyController::Get();
    power_prefs_ = ShellTestApi().power_prefs();

    // Advance the clock an arbitrary amount of time so it won't report zero.
    tick_clock_.Advance(base::TimeDelta::FromSeconds(1));
    power_prefs_->set_tick_clock_for_test(&tick_clock_);

    // Get to Login screen.
    GetSessionControllerClient()->SetSessionState(SessionState::LOGIN_PRIMARY);

    SetUpLocalState();
  }

  void TearDown() override {
    power_prefs_->local_state_ = nullptr;

    NoSessionAshTestBase::TearDown();
  }

  void SetUpLocalState() {
    auto pref_notifier = std::make_unique<PrefNotifierImpl>();
    auto pref_value_store = std::make_unique<PrefValueStore>(
        managed_pref_store_.get() /* managed_prefs */,
        nullptr /* supervised_user_prefs */, nullptr /* extension_prefs */,
        nullptr /* command_line_prefs */, user_pref_store_.get(),
        nullptr /* recommended_prefs */, pref_registry_->defaults().get(),
        pref_notifier.get());
    local_state_ = std::make_unique<PrefService>(
        std::move(pref_notifier), std::move(pref_value_store), user_pref_store_,
        pref_registry_, base::DoNothing(), false);

    PowerPrefs::RegisterLocalStatePrefs(pref_registry_.get());

    power_prefs_->local_state_ = local_state_.get();
    power_prefs_->ObserveLocalStatePrefs(power_prefs_->local_state_);
  }

  std::string GetCurrentPowerPolicy() const {
    return chromeos::PowerPolicyController::GetPolicyDebugString(
        power_manager_client()->policy());
  }

  bool GetCurrentAllowScreenWakeLocks() const {
    return power_policy_controller_->honor_screen_wake_locks_for_test();
  }

  std::vector<power_manager::PowerManagementPolicy_Action>
  GetCurrentPowerPolicyActions() const {
    return {power_manager_client()->policy().ac_idle_action(),
            power_manager_client()->policy().battery_idle_action(),
            power_manager_client()->policy().lid_closed_action()};
  }

  void SetLockedState(ScreenLockState lock_state) {
    GetSessionControllerClient()->SetSessionState(
        lock_state == ScreenLockState::LOCKED ? SessionState::LOCKED
                                              : SessionState::ACTIVE);
  }

  void NotifyScreenIdleOffChanged(bool off) {
    power_manager::ScreenIdleState proto;
    proto.set_off(off);
    power_manager_client()->SendScreenIdleStateChanged(proto);
  }

  PrefService* local_state() { return local_state_.get(); }

  chromeos::PowerPolicyController* power_policy_controller_ =
      nullptr;                         // Not owned.
  PowerPrefs* power_prefs_ = nullptr;  // Not owned.
  base::SimpleTestTickClock tick_clock_;

  scoped_refptr<TestingPrefStore> user_pref_store_ =
      base::MakeRefCounted<TestingPrefStore>();
  scoped_refptr<TestingPrefStore> managed_pref_store_ =
      base::MakeRefCounted<TestingPrefStore>();
  scoped_refptr<PrefRegistrySimple> pref_registry_ =
      base::MakeRefCounted<PrefRegistrySimple>();

  std::unique_ptr<PrefService> local_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerPrefsTest);
};

TEST_F(PowerPrefsTest, LoginScreen) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  EXPECT_EQ(GetSigninScreenPrefService(), prefs);
  EXPECT_EQ(GetExpectedPowerPolicyForPrefs(prefs, ScreenLockState::UNLOCKED),
            GetCurrentPowerPolicy());
  EXPECT_EQ(GetExpectedAllowScreenWakeLocksForPrefs(prefs),
            GetCurrentAllowScreenWakeLocks());

  // Lock the screen and check that the expected delays are used.
  SetLockedState(ScreenLockState::LOCKED);
  EXPECT_EQ(GetExpectedPowerPolicyForPrefs(prefs, ScreenLockState::LOCKED),
            GetCurrentPowerPolicy());

  // Unlock the screen.
  SetLockedState(ScreenLockState::UNLOCKED);
  EXPECT_EQ(GetExpectedPowerPolicyForPrefs(prefs, ScreenLockState::UNLOCKED),
            GetCurrentPowerPolicy());
}

TEST_F(PowerPrefsTest, UserSession) {
  const char kUserEmail[] = "user@example.net";
  SimulateUserLogin(kUserEmail);
  PrefService* prefs = GetUserPrefService(kUserEmail);
  ASSERT_TRUE(prefs);
  EXPECT_EQ(GetExpectedPowerPolicyForPrefs(prefs, ScreenLockState::UNLOCKED),
            GetCurrentPowerPolicy());
  EXPECT_EQ(GetExpectedAllowScreenWakeLocksForPrefs(prefs),
            GetCurrentAllowScreenWakeLocks());
}

TEST_F(PowerPrefsTest, PrimaryUserPrefs) {
  // Add a user with restrictive prefs.
  const char kFirstUserEmail[] = "user1@example.net";
  SimulateUserLogin(kFirstUserEmail);
  PrefService* first_prefs = GetUserPrefService(kFirstUserEmail);
  ASSERT_TRUE(first_prefs);
  first_prefs->SetBoolean(prefs::kPowerAllowScreenWakeLocks, false);
  first_prefs->SetInteger(prefs::kPowerLidClosedAction,
                          chromeos::PowerPolicyController::ACTION_SHUT_DOWN);

  // Add a second user with lenient prefs.
  const char kSecondUserEmail[] = "user2@example.net";
  SimulateUserLogin(kSecondUserEmail);
  PrefService* second_prefs = GetUserPrefService(kSecondUserEmail);
  ASSERT_TRUE(second_prefs);
  second_prefs->SetBoolean(prefs::kPowerAllowScreenWakeLocks, true);
  second_prefs->SetInteger(prefs::kPowerLidClosedAction,
                           chromeos::PowerPolicyController::ACTION_DO_NOTHING);

  // Even though the second user is active, the first (primary) user's prefs
  // should still be used.
  ASSERT_EQ(second_prefs,
            Shell::Get()->session_controller()->GetActivePrefService());
  EXPECT_EQ(
      GetExpectedPowerPolicyForPrefs(first_prefs, ScreenLockState::UNLOCKED),
      GetCurrentPowerPolicy());
}

TEST_F(PowerPrefsTest, AvoidLockDelaysAfterInactivity) {
  const char kUserEmail[] = "user@example.net";
  SimulateUserLogin(kUserEmail);
  PrefService* prefs = GetUserPrefService(kUserEmail);
  ASSERT_TRUE(prefs);
  EXPECT_EQ(GetExpectedPowerPolicyForPrefs(prefs, ScreenLockState::UNLOCKED),
            GetCurrentPowerPolicy());

  // If the screen was already off due to inactivity when it was locked, we
  // should continue using the unlocked delays.
  NotifyScreenIdleOffChanged(true);
  tick_clock_.Advance(base::TimeDelta::FromSeconds(5));
  SetLockedState(ScreenLockState::LOCKED);
  EXPECT_EQ(GetExpectedPowerPolicyForPrefs(prefs, ScreenLockState::UNLOCKED),
            GetCurrentPowerPolicy());

  // If the screen turns on while still locked, we should switch to the locked
  // delays.
  tick_clock_.Advance(base::TimeDelta::FromSeconds(5));
  NotifyScreenIdleOffChanged(false);
  EXPECT_EQ(GetExpectedPowerPolicyForPrefs(prefs, ScreenLockState::LOCKED),
            GetCurrentPowerPolicy());

  tick_clock_.Advance(base::TimeDelta::FromSeconds(5));
  SetLockedState(ScreenLockState::UNLOCKED);
  EXPECT_EQ(GetExpectedPowerPolicyForPrefs(prefs, ScreenLockState::UNLOCKED),
            GetCurrentPowerPolicy());
}

TEST_F(PowerPrefsTest, DisabledLockScreen) {
  const char kUserEmail[] = "user@example.net";
  SimulateUserLogin(kUserEmail);
  PrefService* prefs = GetUserPrefService(kUserEmail);
  ASSERT_TRUE(prefs);

  // Verify that the power policy actions are set to default values initially.
  EXPECT_EQ(std::vector<power_manager::PowerManagementPolicy_Action>(
                3, power_manager::PowerManagementPolicy_Action_SUSPEND),
            GetCurrentPowerPolicyActions());

  // The automatic screen locking is enabled, but, as the lock screen is
  // allowed, the power policy actions still have the default values.
  prefs->SetBoolean(prefs::kEnableAutoScreenLock, true);
  EXPECT_EQ(std::vector<power_manager::PowerManagementPolicy_Action>(
                3, power_manager::PowerManagementPolicy_Action_SUSPEND),
            GetCurrentPowerPolicyActions());

  // The lock screen is disabled, but, as automatic screen locking is not
  // enabled, the power policy actions still have the default values.
  prefs->ClearPref(prefs::kEnableAutoScreenLock);
  prefs->SetBoolean(prefs::kAllowScreenLock, false);
  EXPECT_EQ(std::vector<power_manager::PowerManagementPolicy_Action>(
                3, power_manager::PowerManagementPolicy_Action_SUSPEND),
            GetCurrentPowerPolicyActions());

  // The automatic screen locking is enabled and the lock screen is disabled, so
  // the power policy actions are set now to stop the user session.
  prefs->SetBoolean(prefs::kEnableAutoScreenLock, true);
  EXPECT_EQ(std::vector<power_manager::PowerManagementPolicy_Action>(
                3, power_manager::PowerManagementPolicy_Action_STOP_SESSION),
            GetCurrentPowerPolicyActions());
}

TEST_F(PowerPrefsTest, SmartDimEnabled) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kPowerSmartDimEnabled));
}

TEST_F(PowerPrefsTest, PeakShift) {
  constexpr char kDayConfigsJson[] =
      R"({
        "entries": [
          {
            "charge_start_time": {
               "hour": 20,
               "minute": 0
            },
            "day": "MONDAY",
            "end_time": {
               "hour": 10,
               "minute": 15
            },
            "start_time": {
               "hour": 7,
               "minute": 30
            }
          },
          {
            "charge_start_time": {
               "hour": 22,
               "minute": 30
            },
            "day": "FRIDAY",
            "end_time": {
               "hour": 9,
               "minute": 45
            },
            "start_time": {
               "hour": 4,
               "minute": 0
            }
          }
        ]
      })";
  base::Value day_configs;
  DecodeJsonStringAndNormalize(kDayConfigsJson, &day_configs);

  managed_pref_store_->SetBoolean(prefs::kPowerPeakShiftEnabled, true);
  managed_pref_store_->SetInteger(prefs::kPowerPeakShiftBatteryThreshold, 50);
  managed_pref_store_->SetValue(
      prefs::kPowerPeakShiftDayConfig,
      std::make_unique<base::Value>(std::move(day_configs)), 0);

  EXPECT_EQ(chromeos::PowerPolicyController::GetPeakShiftPolicyDebugString(
                power_manager_client()->policy()),
            GetExpectedPeakShiftPolicyForPrefs(local_state()));
}

TEST_F(PowerPrefsTest, AdvancedBatteryChargeMode) {
  constexpr char kDayConfigsJson[] =
      R"({
        "entries": [
          {
            "charge_start_time": {
               "hour": 15,
               "minute": 15
            },
            "charge_end_time": {
               "hour": 21,
               "minute": 45
            },
            "day": "TUESDAY"
          },
          {
            "charge_start_time": {
               "hour": 10,
               "minute": 30
            },
            "charge_end_time": {
               "hour": 23,
               "minute": 0
            },
            "day": "SUNDAY"
          }
        ]
      })";
  base::Value day_configs;
  DecodeJsonStringAndNormalize(kDayConfigsJson, &day_configs);

  managed_pref_store_->SetBoolean(prefs::kAdvancedBatteryChargeModeEnabled,
                                  true);
  managed_pref_store_->SetValue(
      prefs::kAdvancedBatteryChargeModeDayConfig,
      std::make_unique<base::Value>(std::move(day_configs)), 0);

  EXPECT_EQ(chromeos::PowerPolicyController::
                GetAdvancedBatteryChargeModePolicyDebugString(
                    power_manager_client()->policy()),
            GetExpectedAdvancedBatteryChargeModePolicyForPrefs(local_state()));
}

TEST_F(PowerPrefsTest, BatteryChargeMode) {
  const auto& battery_charge_mode =
      power_manager_client()->policy().battery_charge_mode();

  managed_pref_store_->SetInteger(prefs::kBatteryChargeMode, 2);
  EXPECT_EQ(
      battery_charge_mode.mode(),
      power_manager::PowerManagementPolicy::BatteryChargeMode::EXPRESS_CHARGE);

  managed_pref_store_->SetInteger(prefs::kBatteryChargeMode, 5);
  managed_pref_store_->SetInteger(prefs::kBatteryChargeCustomStartCharging, 55);
  managed_pref_store_->SetInteger(prefs::kBatteryChargeCustomStopCharging, 87);
  EXPECT_EQ(battery_charge_mode.mode(),
            power_manager::PowerManagementPolicy::BatteryChargeMode::CUSTOM);
  EXPECT_EQ(battery_charge_mode.custom_charge_start(), 55);
  EXPECT_EQ(battery_charge_mode.custom_charge_stop(), 87);
}

TEST_F(PowerPrefsTest, BootOnAc) {
  managed_pref_store_->SetBoolean(prefs::kBootOnAcEnabled, true);
  EXPECT_TRUE(power_manager_client()->policy().boot_on_ac());

  managed_pref_store_->SetBoolean(prefs::kBootOnAcEnabled, false);
  EXPECT_FALSE(power_manager_client()->policy().boot_on_ac());
}

TEST_F(PowerPrefsTest, UsbPowerShare) {
  managed_pref_store_->SetBoolean(prefs::kUsbPowerShareEnabled, true);
  EXPECT_TRUE(power_manager_client()->policy().usb_power_share());

  managed_pref_store_->SetBoolean(prefs::kUsbPowerShareEnabled, false);
  EXPECT_FALSE(power_manager_client()->policy().usb_power_share());
}

TEST_F(PowerPrefsTest, AlsLoggingEnabled) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kPowerAlsLoggingEnabled));
}

}  // namespace ash
