// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/brightness_controller_chromeos.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_type.h"

namespace ash {

namespace {

power_manager::PowerSupplyProperties BuildFakePowerSupplyProperties(
    power_manager::PowerSupplyProperties::ExternalPower charger_state) {
  power_manager::PowerSupplyProperties fake_power;
  fake_power.set_external_power(charger_state);
  fake_power.set_battery_percent(50);
  return fake_power;
}

void SetBatteryPower() {
  DCHECK(PowerStatus::IsInitialized());
  PowerStatus::Get()->SetProtoForTesting(BuildFakePowerSupplyProperties(
      power_manager::PowerSupplyProperties::DISCONNECTED));
}

void SetChargerPower() {
  DCHECK(PowerStatus::IsInitialized());
  PowerStatus::Get()->SetProtoForTesting(
      BuildFakePowerSupplyProperties(power_manager::PowerSupplyProperties::AC));
}

bool HasBrightnessPrefValue(const user_manager::KnownUser& known_user,
                            const AccountId& account_id) {
  const base::Value* pref_value = known_user.FindPath(
      account_id, prefs::kInternalDisplayScreenBrightnessPercent);
  if (!pref_value) {
    return false;
  }

  return pref_value->is_double();
}

// Gets the KnownUser internal display brightness pref value. Only call this
// function if HasBrightnessPrefValue is true.
double GetBrightnessPrefValue(const user_manager::KnownUser& known_user,
                              const AccountId& account_id) {
  return known_user
      .FindPath(account_id, prefs::kInternalDisplayScreenBrightnessPercent)
      ->GetDouble();
}

// Return true if there is a KnownUser pref for the ambient light sensor
// disabled reason.
bool HasAmbientLightSensorDisabledReasonPrefValue(
    const user_manager::KnownUser& known_user,
    const AccountId& account_id) {
  const base::Value* pref_value =
      known_user.FindPath(account_id, prefs::kAmbientLightSensorDisabledReason);
  if (!pref_value) {
    return false;
  }

  return pref_value->is_int();
}

// Gets the KnownUser ambient light sensor disabled reason pref value. Only call
// this function if HasAmbientLightSensorDisabledReasonPrefValue is true.
int GetAmbientLightSensorDisabledReasonPrefValue(
    const user_manager::KnownUser& known_user,
    const AccountId& account_id) {
  return known_user
      .FindPath(account_id, prefs::kAmbientLightSensorDisabledReason)
      ->GetInt();
}

// Return true if there is a KnownUser pref for the ambient light sensor
// enabled status.
bool HasDisplayAmbientLightSensorEnabledPrefValue(
    const user_manager::KnownUser& known_user,
    const AccountId& account_id) {
  const base::Value* pref_value =
      known_user.FindPath(account_id, prefs::kDisplayAmbientLightSensorEnabled);
  if (!pref_value) {
    return false;
  }

  return pref_value->is_bool();
}

// Gets the KnownUser ambient light sensor enabled pref value. Only call
// this function if HasDisplayAmbientLightSensorEnabledPrefValue is true.
bool GetDisplayAmbientLightSensorEnabledPrefValue(
    const user_manager::KnownUser& known_user,
    const AccountId& account_id) {
  return known_user
      .FindPath(account_id, prefs::kDisplayAmbientLightSensorEnabled)
      ->GetBool();
}

constexpr char kUserEmail[] = "user@example.com";
constexpr char kUserEmailSecondary[] = "user2@example.com";
constexpr char kUserEmailTertiary[] = "user3@example.com";
constexpr double kInitialBrightness = 51.0;

}  // namespace

class BrightnessControllerChromeosTest : public AshTestBase {
 public:
  BrightnessControllerChromeosTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override { AshTestBase::TearDown(); }

  BrightnessControlDelegate* brightness_control_delegate() {
    return Shell::Get()->brightness_control_delegate();
  }

  LoginDataDispatcher* login_data_dispatcher() {
    return Shell::Get()->login_screen_controller()->data_dispatcher();
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::RunLoop run_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;

  void AdvanceClock(base::TimeDelta time) {
    task_environment()->AdvanceClock(time);
  }

  void SetAmbientLightSensorEnabled(
      bool enabled,
      power_manager::AmbientLightSensorChange_Cause cause) {
    brightness_control_delegate()->SetAmbientLightSensorEnabled(
        enabled, BrightnessControlDelegate::
                     AmbientLightSensorEnabledChangeSource::kSettingsApp);
    power_manager::AmbientLightSensorChange sensor_change;
    sensor_change.set_sensor_enabled(enabled);
    sensor_change.set_cause(cause);
    power_manager_client()->SendAmbientLightSensorEnabledChanged(sensor_change);
  }

  void SetBrightness(double brightness_percent,
                     power_manager::BacklightBrightnessChange_Cause cause) {
    brightness_control_delegate()->HandleBrightnessDown();
    power_manager::BacklightBrightnessChange brightness_change;
    brightness_change.set_percent(brightness_percent);
    brightness_change.set_cause(cause);
    power_manager_client()->set_screen_brightness_percent(brightness_percent);
    power_manager_client()->SendScreenBrightnessChanged(brightness_change);
  }

  void ExpectAmbientLightSensorEnabled(bool expected_value,
                                       const std::string error_message) {
    brightness_control_delegate()->GetAmbientLightSensorEnabled(
        base::BindLambdaForTesting([expected_value, error_message](
                                       std::optional<bool> sensor_enabled) {
          EXPECT_EQ(sensor_enabled.value(), expected_value) << error_message;
        }));
  }

  void ExpectBrightnessPercent(double expected_value,
                               const std::string error_message) {
    brightness_control_delegate()->GetBrightnessPercent(
        base::BindLambdaForTesting(
            [expected_value,
             error_message](std::optional<double> brightness_percent) {
              EXPECT_EQ(brightness_percent.value(), expected_value)
                  << error_message;
            }));
  }

  // On the login screen, focus the given account.
  void LoginScreenFocusAccount(const AccountId account_id) {
    login_data_dispatcher()->NotifyFocusPod(account_id);
    run_loop_.RunUntilIdle();
  }
};

TEST_F(BrightnessControllerChromeosTest, Prefs_OnLoginScreen_MultipleUsers) {
  // Set initial brightness.
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, focus a user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Create a KnownUser for this Local State.
  user_manager::KnownUser known_user(local_state());

  // Simulate the brightness changing automatically due to inactivity.
  // This should not be saved into Local State.
  SetBrightness(0.0,
                power_manager::BacklightBrightnessChange_Cause_USER_INACTIVITY);
  EXPECT_FALSE(HasBrightnessPrefValue(known_user, account_id));

  // Simulate the brightness changing automatically due to activity.
  // This should not be saved into Local State.
  SetBrightness(60.0,
                power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY);
  EXPECT_FALSE(HasBrightnessPrefValue(known_user, account_id));

  // The brightness Local State pref should not be set yet, because the
  // brightness has not yet been adjusted by the user.
  EXPECT_FALSE(HasBrightnessPrefValue(known_user, account_id));

  // The secondary account should not have a pref saved yet either.
  AccountId secondary_account_id =
      AccountId::FromUserEmail(kUserEmailSecondary);
  EXPECT_FALSE(HasBrightnessPrefValue(known_user, secondary_account_id));

  // Set the brightness to a new value via user action.
  double first_brightness_change_percent = 12.0;
  SetBrightness(first_brightness_change_percent,
                power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);

  // The brightness Local State pref should now be set, with the value of the
  // current brightness.
  EXPECT_EQ(GetBrightnessPrefValue(known_user, account_id),
            first_brightness_change_percent);

  // The second user should still not have a pref set.
  EXPECT_FALSE(HasBrightnessPrefValue(known_user, secondary_account_id));

  // On the login screen, focus the second user.
  login_data_dispatcher()->NotifyFocusPod(secondary_account_id);

  // Switching focused users should not change the saved prefs.
  EXPECT_EQ(GetBrightnessPrefValue(known_user, account_id),
            first_brightness_change_percent);

  // The second user should still not have a pref set.
  EXPECT_FALSE(HasBrightnessPrefValue(known_user, secondary_account_id));

  // Set the brightness to a new value via user action.
  double second_brightness_change_percent = 99.9;
  SetBrightness(second_brightness_change_percent,
                power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);

  // The first user should still have its old value set.
  EXPECT_EQ(GetBrightnessPrefValue(known_user, account_id),
            first_brightness_change_percent);

  // The second user should have the new value set.
  EXPECT_EQ(GetBrightnessPrefValue(known_user, secondary_account_id),
            second_brightness_change_percent);
}

TEST_F(BrightnessControllerChromeosTest,
       Prefs_OnLoginScreen_BrightnessNotChanged) {
  // Set initial brightness.
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, focus a user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // The brightness Local State pref should not be set yet, because the
  // brightness has not yet been adjusted.
  user_manager::KnownUser known_user(local_state());
  EXPECT_FALSE(HasBrightnessPrefValue(known_user, account_id));

  SimulateUserLogin(kUserEmail);

  // Wait for callback in
  // BrightnessControllerChromeos::OnActiveUserSessionChanged to finish.
  run_loop_.RunUntilIdle();

  // After login, the brightness pref should have a value equal to the initial
  // brightness level.
  EXPECT_EQ(GetBrightnessPrefValue(known_user, account_id), kInitialBrightness);
}

TEST_F(BrightnessControllerChromeosTest,
       Prefs_AfterLogin_BrightnessChange_SaveToLocalStatePref) {
  // Set initial brightness.
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, focus a user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Simulate the brightness changing automatically due to inactivity.
  // This should not be saved into Local State.
  SetBrightness(0.0,
                power_manager::BacklightBrightnessChange_Cause_USER_INACTIVITY);

  // Simulate the brightness changing automatically due to activity.
  // This should not be saved into Local State.
  SetBrightness(62.0,
                power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY);

  // Set the brightness to a new value via user action.
  double brightness_change_percent = 12.0;
  SetBrightness(brightness_change_percent,
                power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);

  // The brightness Local State pref should now be set, with the value of the
  // current brightness.
  user_manager::KnownUser known_user(local_state());
  EXPECT_EQ(GetBrightnessPrefValue(known_user, account_id),
            brightness_change_percent);

  SimulateUserLogin(kUserEmail);

  // Wait for callback in
  // BrightnessControllerChromeos::OnActiveUserSessionChanged to finish.
  run_loop_.RunUntilIdle();

  // Simulate the brightness changing automatically due to inactivity.
  // This should not be saved into Local State.
  SetBrightness(0.0,
                power_manager::BacklightBrightnessChange_Cause_USER_INACTIVITY);

  // Confirm that after login, the brightness Local State pref has the same
  // value.
  EXPECT_EQ(GetBrightnessPrefValue(known_user, account_id),
            brightness_change_percent);

  // Change the brightness via user request (this time, after login).
  double new_brightness_change_percent = 96.0;
  SetBrightness(new_brightness_change_percent,
                power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);

  // The Local State brightness pref should have the new brightness value
  // stored.
  EXPECT_EQ(GetBrightnessPrefValue(known_user, account_id),
            new_brightness_change_percent);

  // Change the brightness via user request, from the Settings app.
  double settings_brightness_change_percent = 22.2;
  SetBrightness(
      settings_brightness_change_percent,
      power_manager::
          BacklightBrightnessChange_Cause_USER_REQUEST_FROM_SETTINGS_APP);

  // The Local State brightness pref should have the new brightness value
  // stored.
  EXPECT_EQ(GetBrightnessPrefValue(known_user, account_id),
            settings_brightness_change_percent);
}

TEST_F(BrightnessControllerChromeosTest,
       HistogramTest_DecreaseBrightnessOnLoginScreen) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      0);

  // Start on the login screen
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // "Unplug" the device from charger
  SetBatteryPower();

  // Wait for a period of time, then send a brightness event
  int seconds_to_wait = 11;
  AdvanceClock(base::Seconds(seconds_to_wait));
  brightness_control_delegate()->HandleBrightnessDown();

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      base::Seconds(seconds_to_wait), 1);

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for a period of time, then send another brightness event
  AdvanceClock(base::Seconds(5));
  brightness_control_delegate()->HandleBrightnessDown();

  // The number of events should not have changed, since we already recorded a
  // metric on the login screen.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
}

TEST_F(BrightnessControllerChromeosTest, HistogramTest_LoginSecondary) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      0);

  // Start on the "secondary" login screen (i.e. another user is already
  // logged-in, and another user is now logging in).
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);

  // "Unplug" the device from charger
  SetBatteryPower();

  // Wait for a period of time, then send a brightness event
  int seconds_to_wait = 22;
  AdvanceClock(base::Seconds(seconds_to_wait));
  brightness_control_delegate()->HandleBrightnessDown();

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      base::Seconds(seconds_to_wait), 1);

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for a period of time, then send another brightness event
  AdvanceClock(base::Seconds(5));
  brightness_control_delegate()->HandleBrightnessDown();

  // The number of events should not have changed, since we already recorded a
  // metric on the login screen.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
}

TEST_F(BrightnessControllerChromeosTest, HistogramTest_PowerSourceCharger) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "IncreaseBrightness.ChargerPower",
      0);

  // Start on the login screen
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // "Plug in" the charger
  SetChargerPower();

  // Wait for a period of time, then send a brightness event
  int seconds_to_wait = 8;
  AdvanceClock(base::Seconds(seconds_to_wait));
  brightness_control_delegate()->HandleBrightnessUp();

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "IncreaseBrightness.ChargerPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "IncreaseBrightness.ChargerPower",
      base::Seconds(seconds_to_wait), 1);
}

TEST_F(BrightnessControllerChromeosTest,
       HistogramTest_BrightnessChangeAfterLogin) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "SetBrightness.ChargerPower",
      0);

  // Start on the login screen
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // "Plug in" the charger
  SetChargerPower();

  // Wait for a period of time, but don't change brightness.
  AdvanceClock(base::Seconds(9));

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for a period of time, then send another brightness event
  int seconds_to_wait = 5;
  AdvanceClock(base::Seconds(seconds_to_wait));
  brightness_control_delegate()->SetBrightnessPercent(
      50, /*gradual=*/true, /*source=*/
      BrightnessControlDelegate::BrightnessChangeSource::kQuickSettings);

  // Brightness changes from Quick Settings should have cause "USER_REQUEST".
  EXPECT_EQ(power_manager_client()->requested_screen_brightness_cause(),
            power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);

  // Expect a record with the number of seconds since login, not since the
  // beginning of the login screen.
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "SetBrightness.ChargerPower",
      base::Seconds(seconds_to_wait), 1);
}

TEST_F(BrightnessControllerChromeosTest,
       HistogramTest_BrightnessChangeAfterLogin_LongDuration) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.BatteryPower",
      0);

  // Start on the login screen
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // "Unplug" the charger
  SetBatteryPower();

  // Wait for a period of time, but don't change brightness.
  AdvanceClock(base::Seconds(20));

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for a period of time, then send another brightness event
  int minutes_to_wait = 35;
  AdvanceClock(base::Minutes(minutes_to_wait));
  brightness_control_delegate()->HandleBrightnessUp();

  // Expect a record with the number of minutes since login, not since the
  // beginning of the login screen.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.BatteryPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.BatteryPower",
      base::Minutes(minutes_to_wait), 1);
}

TEST_F(BrightnessControllerChromeosTest,
       HistogramTest_BrightnessChangeAfterLogin_DurationGreaterThanOneHour) {
  // Start on the login screen
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  // "Unplug" the charger
  SetBatteryPower();

  // Wait for a period of time, but don't change brightness.
  AdvanceClock(base::Seconds(1));

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for > 1 hour, then send a brightness event.
  AdvanceClock(base::Hours(1) + base::Minutes(5));
  brightness_control_delegate()->HandleBrightnessUp();

  // Since the brightness event occurred >1 hour after login, don't record it.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.BatteryPower",
      0);
}

TEST_F(BrightnessControllerChromeosTest, SetAmbientLightSensorEnabled) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  SetBatteryPower();

  // Ambient light sensor is enabled by default.
  EXPECT_TRUE(power_manager_client()->is_ambient_light_sensor_enabled());

  // Disable the ambient light sensor via the BrightnessControlDelegate.
  brightness_control_delegate()->SetAmbientLightSensorEnabled(
      false, BrightnessControlDelegate::AmbientLightSensorEnabledChangeSource::
                 kSettingsApp);
  // PowerManagerClient should have been invoked, disabling the ambient light
  // sensor.
  EXPECT_FALSE(power_manager_client()->is_ambient_light_sensor_enabled());

  // Re-enabled the ambient light sensor via the BrightnessControlDelegate.
  brightness_control_delegate()->SetAmbientLightSensorEnabled(
      true, BrightnessControlDelegate::AmbientLightSensorEnabledChangeSource::
                kSettingsApp);
  // PowerManagerClient should have been invoked, re-enabling the ambient light
  // sensor.
  EXPECT_TRUE(power_manager_client()->is_ambient_light_sensor_enabled());
}

TEST_F(BrightnessControllerChromeosTest, GetAmbientLightSensorEnabled) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  SetBatteryPower();

  // Disable the ambient light sensor via the BrightnessControlDelegate.
  brightness_control_delegate()->SetAmbientLightSensorEnabled(
      false, BrightnessControlDelegate::AmbientLightSensorEnabledChangeSource::
                 kSettingsApp);

  // GetAmbientLightSensorEnabled should return that the the light sensor is
  // currently not enabled.
  brightness_control_delegate()->GetAmbientLightSensorEnabled(
      base::BindOnce([](std::optional<bool> is_ambient_light_sensor_enabled) {
        EXPECT_FALSE(is_ambient_light_sensor_enabled.value());
      }));

  // Re-enable the ambient light sensor via the BrightnessControlDelegate.
  brightness_control_delegate()->SetAmbientLightSensorEnabled(
      true, BrightnessControlDelegate::AmbientLightSensorEnabledChangeSource::
                kSettingsApp);

  // GetAmbientLightSensorEnabled should return that the the light sensor is
  // currently enabled.
  brightness_control_delegate()->GetAmbientLightSensorEnabled(
      base::BindOnce([](std::optional<bool> is_ambient_light_sensor_enabled) {
        EXPECT_TRUE(is_ambient_light_sensor_enabled.value());
      }));
}

TEST_F(BrightnessControllerChromeosTest, HasAmbientLightSensor) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  SetBatteryPower();

  power_manager_client()->set_has_ambient_light_sensor(true);

  brightness_control_delegate()->HasAmbientLightSensor(
      base::BindOnce([](std::optional<bool> has_ambient_light_sensor) {
        EXPECT_TRUE(has_ambient_light_sensor.value());
      }));

  power_manager_client()->set_has_ambient_light_sensor(false);

  brightness_control_delegate()->HasAmbientLightSensor(
      base::BindOnce([](std::optional<bool> has_ambient_light_sensor) {
        EXPECT_FALSE(has_ambient_light_sensor.value());
      }));
}

TEST_F(BrightnessControllerChromeosTest, AmbientLightSensorDisabledReasonPref) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  SetBatteryPower();

  // Set the ambient light sensor to be enabled initially.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  // Wait for AmbientLightSensorEnabledChange observer to be notified.
  run_loop_.RunUntilIdle();

  // On the login screen, focus a user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  user_manager::KnownUser known_user(local_state());

  // Confirm that no "disabled reason" pref exists for the given KnownUser.
  EXPECT_FALSE(
      HasAmbientLightSensorDisabledReasonPrefValue(known_user, account_id));

  // Disable the ambient light sensor.
  request.set_sensor_enabled(false);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  // Wait for AmbientLightSensorEnabledChange observer to be notified.
  run_loop_.RunUntilIdle();

  // There should now be a "disabled reason" pref stored in KnownUser.
  EXPECT_TRUE(
      HasAmbientLightSensorDisabledReasonPrefValue(known_user, account_id));
  // In this case, the cause is USER_REQUEST_SETTINGS_APP because the change was
  // made from the PowerManagerClient SetAmbientLightSensorEnabled function.
  EXPECT_EQ(
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP,
      GetAmbientLightSensorDisabledReasonPrefValue(known_user, account_id));

  // Re-enable the ambient light sensor.
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  // Wait for AmbientLightSensorEnabledChange observer to be notified.
  run_loop_.RunUntilIdle();

  // After the ambient light sensor is re-enabled, the "disabled reason" pref
  // should be deleted.
  EXPECT_FALSE(
      HasAmbientLightSensorDisabledReasonPrefValue(known_user, account_id));

  // Get a reference to the actual BrightnessControllerChromeos so that we can
  // trigger its observer callback manually.
  system::BrightnessControllerChromeos* brightness_controller =
      static_cast<system::BrightnessControllerChromeos*>(
          brightness_control_delegate());

  // Disable the ambient light sensor, triggering the observer callback manually
  // so we can manually set a cause.
  {
    power_manager::AmbientLightSensorChange change;
    change.set_sensor_enabled(false);
    power_manager::AmbientLightSensorChange_Cause expected_cause =
        power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST;
    change.set_cause(expected_cause);
    brightness_controller->AmbientLightSensorEnabledChanged(change);
    EXPECT_EQ(expected_cause, GetAmbientLightSensorDisabledReasonPrefValue(
                                  known_user, account_id));
  }

  {
    // Disable the ambient light sensor (again triggering the observer callback
    // manually), choosing a different cause this time. Note that the pref will
    // be set even if the ambient light sensor was previously disabled.
    power_manager::AmbientLightSensorChange change;
    change.set_sensor_enabled(false);
    power_manager::AmbientLightSensorChange_Cause expected_cause =
        power_manager::AmbientLightSensorChange_Cause_NO_READINGS_FROM_ALS;
    change.set_cause(expected_cause);
    brightness_controller->AmbientLightSensorEnabledChanged(change);
    EXPECT_EQ(expected_cause, GetAmbientLightSensorDisabledReasonPrefValue(
                                  known_user, account_id));
  }
}

TEST_F(BrightnessControllerChromeosTest, AmbientLightSensorEnabledPref) {
  // Activate user session.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Set the ambient light sensor to be enabled initially.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  // Wait for AmbientLightSensorEnabledChange observer to be notified.
  run_loop_.RunUntilIdle();

  // User pref is default to true.
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));

  // Disable the sensor via brightness change (not from settings app), pref
  // should remain true.
  SetAmbientLightSensorEnabled(
      false, power_manager::AmbientLightSensorChange_Cause::
                 AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));

  // Re-enable the sensor from settings app, pref should be true.
  SetAmbientLightSensorEnabled(
      true, power_manager::AmbientLightSensorChange_Cause::
                AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));

  // Disable the sensor again, this time, the request is from settings app, the
  // pref should be updated to false.
  SetAmbientLightSensorEnabled(
      false, power_manager::AmbientLightSensorChange_Cause::
                 AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));

  // Re-enable the sensor via user settings and verify the preference updates.
  SetAmbientLightSensorEnabled(
      true, power_manager::AmbientLightSensorChange_Cause::
                AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));
}

class BrightnessControllerChromeosTest_NonApplicableSessionStates
    : public BrightnessControllerChromeosTest,
      public testing::WithParamInterface<session_manager::SessionState> {};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    BrightnessControllerChromeosTest_NonApplicableSessionStates,
    testing::ValuesIn<session_manager::SessionState>({
        session_manager::SessionState::LOCKED,
        session_manager::SessionState::LOGGED_IN_NOT_ACTIVE,
        session_manager::SessionState::OOBE,
        session_manager::SessionState::RMA,
        session_manager::SessionState::UNKNOWN,
    }));

TEST_P(BrightnessControllerChromeosTest_NonApplicableSessionStates,
       HistogramTest_NonApplicableSessionStates) {
  GetSessionControllerClient()->SetSessionState(GetParam());

  // "Plug in" the charger
  SetChargerPower();

  // Wait for a period of time, then send a brightness event
  AdvanceClock(base::Seconds(8));
  brightness_control_delegate()->HandleBrightnessUp();

  // Expect no events, since the event did not happen on the login screen or
  // after login.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "IncreaseBrightness.ChargerPower",
      0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.ChargerPower",
      0);
}

TEST_F(BrightnessControllerChromeosTest,
       HistogramTest_SetBrightnessAfterSystemRestoration) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableBrightnessControlInSettings);

  // Start on the login screen.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  SetBatteryPower();

  // Metrics count should start at 0, both OnLogin and AfterLogin.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "SetBrightness.BatteryPower",
      0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "SetBrightness.BatteryPower",
      0);

  // Log in.
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Set display brightness.
  known_user.SetPath(account_id, prefs::kInternalDisplayScreenBrightnessPercent,
                     std::make_optional<base::Value>(30.0));

  // Simulate reboot, brightness should be restored.
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Verify that system restoring brightness is not recorded.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "SetBrightness.BatteryPower",
      0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "SetBrightness.BatteryPower",
      0);

  // Wait and then simulate a user-initiated brightness change.
  int seconds_to_wait = 5;
  AdvanceClock(base::Seconds(seconds_to_wait));
  brightness_control_delegate()->SetBrightnessPercent(
      50, /*gradual=*/true, /*source=*/
      BrightnessControlDelegate::BrightnessChangeSource::kQuickSettings);

  // Verify that the user-initiated brightness change is recorded.
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Display.TimeUntilFirstBrightnessChange.AfterLogin."
      "SetBrightness.BatteryPower",
      base::Seconds(seconds_to_wait), 1);
}

TEST_F(BrightnessControllerChromeosTest, SetBrightnessPercent_Cause) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  SetChargerPower();

  brightness_control_delegate()->SetBrightnessPercent(
      50, /*gradual=*/true, /*source=*/
      BrightnessControlDelegate::BrightnessChangeSource::kQuickSettings);

  // Brightness changes from Quick Settings should have cause "USER_REQUEST".
  EXPECT_EQ(power_manager_client()->requested_screen_brightness_cause(),
            power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);

  brightness_control_delegate()->SetBrightnessPercent(
      50, /*gradual=*/true, /*source=*/
      BrightnessControlDelegate::BrightnessChangeSource::kSettingsApp);

  // Brightness changes from the Settings app should have cause
  // "USER_REQUEST_FROM_SETTINGS_APP".
  EXPECT_EQ(
      power_manager_client()->requested_screen_brightness_cause(),
      power_manager::
          SetBacklightBrightnessRequest_Cause_USER_REQUEST_FROM_SETTINGS_APP);
}

TEST_F(BrightnessControllerChromeosTest, SetAmbientLightSensorEnabled_Cause) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  SetChargerPower();

  brightness_control_delegate()->SetAmbientLightSensorEnabled(
      true, BrightnessControlDelegate::AmbientLightSensorEnabledChangeSource::
                kSettingsApp);

  // Brightness changes from Setting app should have cause
  // "USER_REQUEST_FROM_SETTINGS_APP".
  EXPECT_EQ(
      power_manager_client()->requested_ambient_light_sensor_enabled_cause(),
      power_manager::
          SetAmbientLightSensorEnabledRequest_Cause_USER_REQUEST_FROM_SETTINGS_APP);

  brightness_control_delegate()->SetAmbientLightSensorEnabled(
      false, BrightnessControlDelegate::AmbientLightSensorEnabledChangeSource::
                 kRestoredFromUserPref);

  // Brightness changes from the Settings app should have cause
  // "USER_REQUEST_FROM_SETTINGS_APP".
  EXPECT_EQ(
      power_manager_client()->requested_ambient_light_sensor_enabled_cause(),
      power_manager::
          SetAmbientLightSensorEnabledRequest_Cause_RESTORED_FROM_USER_PREFERENCE);
}

TEST_F(BrightnessControllerChromeosTest,
       RestoreBrightnessSettingsFromPref_FlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableBrightnessControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);
  run_loop_.RunUntilIdle();

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, focus the first user.
  AccountId first_account = AccountId::FromUserEmail(kUserEmail);
  LoginScreenFocusAccount(first_account);
  ExpectAmbientLightSensorEnabled(
      true, "ALS should be enabled for first user by default.");
  ExpectBrightnessPercent(
      kInitialBrightness,
      "Brightness should be unchanged for first user after initial focus.");

  // Then, focus the second user.
  AccountId second_account = AccountId::FromUserEmail(kUserEmailSecondary);
  LoginScreenFocusAccount(second_account);
  ExpectAmbientLightSensorEnabled(
      true, "ALS should be enabled for second user by default.");
  ExpectBrightnessPercent(
      kInitialBrightness,
      "Brightness should be unchanged for second user after initial focus.");

  // Switch back to the first user, then disable ALS by changing the brightness.
  LoginScreenFocusAccount(first_account);
  const double first_brightness_change_percent = 12.0;
  brightness_control_delegate()->SetBrightnessPercent(
      first_brightness_change_percent, /*gradual=*/false,
      BrightnessControlDelegate::BrightnessChangeSource::kQuickSettings);
  // Wait for callbacks to finish executing.
  run_loop_.RunUntilIdle();
  ExpectAmbientLightSensorEnabled(
      false, "ALS should be disabled for first user after brightness change.");
  ExpectBrightnessPercent(first_brightness_change_percent,
                          "Brightness should be equal to the previously-set "
                          "value for the first user.");

  LoginScreenFocusAccount(second_account);
  ExpectAmbientLightSensorEnabled(true,
                                  "ALS should be enabled for second user "
                                  "despite being disabled for first user.");
  ExpectBrightnessPercent(
      first_brightness_change_percent,
      "Brightness should remain the same after switching to the second user.");

  // Switch to a third user.
  AccountId third_account = AccountId::FromUserEmail(kUserEmailTertiary);
  LoginScreenFocusAccount(third_account);
  ExpectAmbientLightSensorEnabled(true,
                                  "ALS should be enabled for third user "
                                  "despite being disabled for first user.");
  ExpectBrightnessPercent(
      first_brightness_change_percent,
      "Brightness should remain the same after switching to the second user.");

  // Set the brightness for the third user.
  const double second_brightness_change_percent = 77.0;
  brightness_control_delegate()->SetBrightnessPercent(
      second_brightness_change_percent, /*gradual=*/false,
      BrightnessControlDelegate::BrightnessChangeSource::kQuickSettings);
  // Wait for callbacks to finish executing.
  run_loop_.RunUntilIdle();

  ExpectAmbientLightSensorEnabled(
      false, "ALS should be disabled for third user after brightness change.");
  ExpectBrightnessPercent(
      second_brightness_change_percent,
      "Brightness for the third user should be equal to the last-set value.");

  LoginScreenFocusAccount(first_account);
  ExpectAmbientLightSensorEnabled(false,
                                  "ALS should be disabled for first user after "
                                  "switching back from second user.");
  ExpectBrightnessPercent(
      first_brightness_change_percent,
      "Brightness should be equal to the previously-set value for the first "
      "user, even after focusing the second user.");

  LoginScreenFocusAccount(third_account);
  ExpectAmbientLightSensorEnabled(false,
                                  "ALS should be disabled for third user after "
                                  "switching back from first user.");
  ExpectBrightnessPercent(
      second_brightness_change_percent,
      "Brightness should be restored to the previously-set value for the third "
      "user, even after focusing the first user (which has its own set "
      "brightness value).");

  // Simulate a reboot, which resets the value of the ambient light sensor and
  // the screen brightness.
  ClearLogin();
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);

  LoginScreenFocusAccount(second_account);
  ExpectAmbientLightSensorEnabled(
      true, "After reboot, ALS should be still be enabled for second user.");
  ExpectBrightnessPercent(kInitialBrightness,
                          "After reboot, brightness should be equal to the "
                          "initial value for the second user.");

  LoginScreenFocusAccount(first_account);
  ExpectAmbientLightSensorEnabled(
      false, "After reboot, ALS should be still be disabled for first user.");
  ExpectBrightnessPercent(first_brightness_change_percent,
                          "After reboot, brightness should be equal to the "
                          "previously-set value for the first user.");

  LoginScreenFocusAccount(second_account);
  ExpectAmbientLightSensorEnabled(
      true, "After reboot, ALS should be still be enabled for second user.");
  ExpectBrightnessPercent(first_brightness_change_percent,
                          "After switching to the second user (after reboot), "
                          "brightness should be equal to the "
                          "last value set (since auto-brightness is enabled).");

  LoginScreenFocusAccount(first_account);
  ExpectAmbientLightSensorEnabled(
      false,
      "After reboot and after switching back from second user, ALS should be "
      "still be disabled for first user.");
  ExpectBrightnessPercent(
      first_brightness_change_percent,
      "After reboot, brightness should be equal to the previously-set value "
      "for the first user, even after focusing the second user.");

  LoginScreenFocusAccount(third_account);
  ExpectAmbientLightSensorEnabled(
      false, "After reboot, ALS should be still be disabled for third user.");
  ExpectBrightnessPercent(second_brightness_change_percent,
                          "After reboot, brightness should be equal to the "
                          "previously-set value for the third user, even after "
                          "focusing the first user.");
}

TEST_F(BrightnessControllerChromeosTest,
       RestoreBrightnessSettingsFromPref_FlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kEnableBrightnessControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);
  run_loop_.RunUntilIdle();

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, focus the first user.
  AccountId first_account = AccountId::FromUserEmail(kUserEmail);
  LoginScreenFocusAccount(first_account);
  ExpectAmbientLightSensorEnabled(
      true, "ALS should be enabled for first user by default.");
  ExpectBrightnessPercent(
      kInitialBrightness,
      "Brightness should be unchanged for first user after initial focus.");

  // Then, focus the second user.
  AccountId second_account = AccountId::FromUserEmail(kUserEmailSecondary);
  LoginScreenFocusAccount(second_account);
  ExpectAmbientLightSensorEnabled(
      true, "ALS should be enabled for second user by default.");
  ExpectBrightnessPercent(
      kInitialBrightness,
      "Brightness should be unchanged for second user after initial focus.");

  // Switch back to the first user, then disable ALS by changing the brightness.
  LoginScreenFocusAccount(first_account);
  const double brightness_change_percent = 12.0;
  brightness_control_delegate()->SetBrightnessPercent(
      brightness_change_percent, /*gradual=*/false,
      BrightnessControlDelegate::BrightnessChangeSource::kQuickSettings);
  // Wait for callbacks to finish executing.
  run_loop_.RunUntilIdle();
  ExpectAmbientLightSensorEnabled(
      false, "ALS should be disabled for first user after brightness change.");
  ExpectBrightnessPercent(brightness_change_percent,
                          "Brightness should be equal to the previously-set "
                          "value for the first user.");

  LoginScreenFocusAccount(second_account);
  ExpectAmbientLightSensorEnabled(
      false,
      "ALS should be disabled for second user because the ALS value is not "
      "being restored from prefs.");
  ExpectBrightnessPercent(brightness_change_percent,
                          "After switching to the second user, the previous "
                          "brightness should not be restored.");

  LoginScreenFocusAccount(first_account);
  ExpectAmbientLightSensorEnabled(false,
                                  "ALS should be disabled for first user after "
                                  "switching back from second user.");
  ExpectBrightnessPercent(brightness_change_percent,
                          "After switching back to the first user, the "
                          "previous brightness should not be restored.");

  // Simulate a reboot, which resets the value of the ambient light sensor and
  // the screen brightness.
  ClearLogin();
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);

  LoginScreenFocusAccount(first_account);
  ExpectAmbientLightSensorEnabled(
      true, "After reboot, ALS should be enabled for first user by default.");
  ExpectBrightnessPercent(kInitialBrightness,
                          "After reboot, the brightness level should be equal "
                          "to the initial brightness for the first user.");

  LoginScreenFocusAccount(second_account);
  ExpectAmbientLightSensorEnabled(
      true, "After reboot, ALS should be enabled for second user by default.");
  ExpectBrightnessPercent(kInitialBrightness,
                          "After reboot, the brightness level should be equal "
                          "to the initial brightness for the second user.");

  LoginScreenFocusAccount(first_account);
  ExpectAmbientLightSensorEnabled(
      true, "After reboot, ALS should be enabled for first user by default.");
  ExpectBrightnessPercent(kInitialBrightness,
                          "After reboot, the brightness level should be equal "
                          "to the initial brightness for the first user (after "
                          "switching from the second user).");
}

TEST_F(BrightnessControllerChromeosTest,
       ReenableAmbientLightSensor_Reboot_DisabledFromSettingsApp) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableBrightnessControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);
  run_loop_.RunUntilIdle();

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Set ALS to false, and set the disabled reason to be
  // USER_REQUEST_SETTINGS_APP.
  SetAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
  known_user.SetPath(
      account_id, prefs::kAmbientLightSensorDisabledReason,
      std::make_optional<base::Value>(
          power_manager::
              AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP));

  // ALS is disabled.
  ExpectAmbientLightSensorEnabled(
      false,
      "Ambient light sensor is disabled, the request is from settings app.");
  EXPECT_EQ(false, GetDisplayAmbientLightSensorEnabledPrefValue(known_user,
                                                                account_id));

  // "disabled reason" pref stored in KnownUser should be
  // USER_REQUEST_SETTINGS_APP.
  EXPECT_TRUE(
      HasAmbientLightSensorDisabledReasonPrefValue(known_user, account_id));
  EXPECT_EQ(
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP,
      GetAmbientLightSensorDisabledReasonPrefValue(known_user, account_id));

  // Simulate reboot, and log in again.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  known_user.SetPath(account_id, prefs::kInternalDisplayScreenBrightnessPercent,
                     std::make_optional<base::Value>(30.0));
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Expect ambient light sensor remain disabled, and brightness should be
  // restored.
  ExpectAmbientLightSensorEnabled(false, "ALS remain disabled");
  ExpectBrightnessPercent(30.0, "Brightness percent should be restored.");

  // Simulate reboot, and log in the third time.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // ALS and brightness should remain the same as last reboot.
  ExpectAmbientLightSensorEnabled(false, "ALS should remain disabled.");
  ExpectBrightnessPercent(30.0, "Brightness percent should be restored.");
}
TEST_F(BrightnessControllerChromeosTest,
       ReenableAmbientLightSensor_Reboot_DisabledFromBrightnessKey) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableBrightnessControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);
  run_loop_.RunUntilIdle();

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Set ALS to false, and set the disabled reason to be
  // BRIGHTNESS_USER_REQUEST.
  SetAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
  known_user.SetPath(
      account_id, prefs::kAmbientLightSensorDisabledReason,
      std::make_optional<base::Value>(
          power_manager::
              AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST));

  // ALS is disabled.
  ExpectAmbientLightSensorEnabled(
      false,
      "Ambient light sensor is disabled, the request is from settings app.");
  EXPECT_EQ(false, GetDisplayAmbientLightSensorEnabledPrefValue(known_user,
                                                                account_id));

  // "disabled reason" pref stored in KnownUser should be
  // BRIGHTNESS_USER_REQUEST.
  EXPECT_TRUE(
      HasAmbientLightSensorDisabledReasonPrefValue(known_user, account_id));
  EXPECT_EQ(
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST,
      GetAmbientLightSensorDisabledReasonPrefValue(known_user, account_id));

  // Simulate reboot, and log in again.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  known_user.SetPath(account_id, prefs::kInternalDisplayScreenBrightnessPercent,
                     std::make_optional<base::Value>(30.0));
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Expect ambient light sensor is re-enabled.
  ExpectAmbientLightSensorEnabled(true, "ALS is re-enabled.");

  // Simulate reboot, and log in the third time.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // ALS should remain enabled.
  ExpectAmbientLightSensorEnabled(
      true, "ALS should remain enabled after re-enabled in last reboot.");
}

TEST_F(BrightnessControllerChromeosTest,
       BrightnessSettingsUnchanged_DeviceLocked) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableBrightnessControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);
  run_loop_.RunUntilIdle();

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Disable ALS using the brightness key.
  SetAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);

  // Current status: Als is turned off, and current brightness is
  // kInitialBrightness.
  ExpectAmbientLightSensorEnabled(
      false,
      "Ambient light sensor is disabled, the request is from brightness key.");
  ExpectBrightnessPercent(kInitialBrightness,
                          "Current brightness should be kInitialBrightness.");

  // Simulate device lock and re-login.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Als should not be re-enabled, although it was not previously disabled from
  // settings app. The brightness percent should still be kInitialBrightness.
  ExpectAmbientLightSensorEnabled(false, "ALS remain disabled.");
  ExpectBrightnessPercent(kInitialBrightness,
                          "Brightness should remain unchanged.");
}

TEST_F(BrightnessControllerChromeosTest,
       RestoreAutoBrightnessForNewUser_FlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableBrightnessControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);
  run_loop_.RunUntilIdle();

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, select and login with an existing user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  login_data_dispatcher()->NotifyFocusPod(account_id);
  LoginScreenFocusAccount(account_id);
  SimulateUserLogin(kUserEmail);

  // The ambient light sensor should be enabled by default.
  ExpectAmbientLightSensorEnabled(
      true, "Ambient light sensor should be enabled by default.");

  // Verify that the synced ambient light sensor profile pref value has a
  // default value of true.
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));
  // There should not be a KnownUser pref set initially because there hasn't
  // been a change to the ambient light sensor status yet.
  user_manager::KnownUser known_user(local_state());
  EXPECT_FALSE(
      HasDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));

  // Disable the ambient light sensor by manually changing the brightness.
  brightness_control_delegate()->HandleBrightnessDown();
  // Wait for AmbientLightSensorEnabledChange observer to be notified.
  run_loop_.RunUntilIdle();

  ExpectAmbientLightSensorEnabled(
      false,
      "Ambient light sensor should be disabled for first user after manually "
      "changing the brightness.");

  // After the ambient light sensor status is disabled, the KnownUser pref
  // should be stored with the correct value (false).
  EXPECT_TRUE(
      HasDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));
  EXPECT_FALSE(
      GetDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));
  // The synced profile pref should also have the correct value (false).
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));

  // Simulate a reboot, which resets the value of the ambient light sensor and
  // the screen brightness.
  ClearLogin();
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);

  // Simulate a login with a second user, as if it's that user's first time
  // logging in on this device.
  SimulateNewUserFirstLogin(kUserEmailSecondary);

  // The value of the synced profile pref for the ambient light sensor should be
  // true by default, and the ambient light sensor should be enabled.
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));
  ExpectAmbientLightSensorEnabled(
      true, "Ambient light sensor should be enabled for new users.");

  // Before logging in the first user, manually set the synced pref to false to
  // simulate the pref finishing syncing to the new device.
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref_service->SetBoolean(prefs::kDisplayAmbientLightSensorLastEnabled, false);

  // Now, login the first user again, as if it's that user's first time
  // logging in on this device.
  SimulateNewUserFirstLogin(kUserEmail);

  // The value of the synced profile pref for the ambient light sensor should be
  // false, because on the "other device" that value was set to false.
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));
  // As a result, the local state pref for ambient light sensor status should be
  // disabled, and the ambient light sensor should be disabled.
  EXPECT_TRUE(
      HasDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));
  EXPECT_FALSE(
      GetDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));
  ExpectAmbientLightSensorEnabled(
      false,
      "Ambient light sensor should be disabled for first user after logging in "
      "from a new device.");
}

TEST_F(BrightnessControllerChromeosTest,
       RestoreAutoBrightnessForNewUser_FlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kEnableBrightnessControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);
  run_loop_.RunUntilIdle();

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, select and login with an existing user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  login_data_dispatcher()->NotifyFocusPod(account_id);
  LoginScreenFocusAccount(account_id);
  SimulateUserLogin(kUserEmail);

  // The ambient light sensor should be enabled by default.
  ExpectAmbientLightSensorEnabled(
      true, "Ambient light sensor should be enabled by default.");

  // Verify that the synced ambient light sensor profile pref value has a
  // default value of true.
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));
  // There should not be a KnownUser pref set initially because there hasn't
  // been a change to the ambient light sensor status yet.
  user_manager::KnownUser known_user(local_state());
  EXPECT_FALSE(
      HasDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));

  // Disable the ambient light sensor by manually changing the brightness.
  brightness_control_delegate()->HandleBrightnessDown();
  // Wait for AmbientLightSensorEnabledChange observer to be notified.
  run_loop_.RunUntilIdle();

  ExpectAmbientLightSensorEnabled(
      false,
      "Ambient light sensor should be disabled for first user after manually "
      "changing the brightness.");

  // After the ambient light sensor status is disabled, the KnownUser pref
  // should be stored with the correct value (false).
  EXPECT_TRUE(
      HasDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));
  EXPECT_FALSE(
      GetDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));
  // The synced profile pref should also have the correct value (false).
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));

  // Simulate a reboot, which resets the value of the ambient light sensor and
  // the screen brightness.
  ClearLogin();
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);

  // Simulate a login with a second user, as if it's that user's first time
  // logging in on this device.
  SimulateNewUserFirstLogin(kUserEmailSecondary);

  // The value of the synced profile pref for the ambient light sensor should be
  // true by default, and the ambient light sensor should be enabled.
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));
  ExpectAmbientLightSensorEnabled(
      true, "Ambient light sensor should be enabled for new users.");

  // Before logging in the first user, manually set the synced pref to false to
  // simulate the pref finishing syncing to the new device.
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref_service->SetBoolean(prefs::kDisplayAmbientLightSensorLastEnabled, false);

  // Now, login the first user again, as if it's that user's first time
  // logging in on this device.
  SimulateNewUserFirstLogin(kUserEmail);

  // The value of the synced profile pref for the ambient light sensor should be
  // false, because on the "other device" that value was set to false.
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));
  // However, because the brightness-control flag is disabled, the ambient light
  // sensor preference will not be restored, and thus the ambient light sensor
  // should be disabled.
  ExpectAmbientLightSensorEnabled(
      true,
      "Ambient light sensor should be enabled for first user after logging in "
      "from a new device.");
}

TEST_F(BrightnessControllerChromeosTest,
       RestoreBrightnessSettings_ScreenBrightnessPercentPolicySet) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableBrightnessControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);
  run_loop_.RunUntilIdle();

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Set brightness to 100%.
  SetAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
  SetBrightness(100.0,
                power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  ExpectBrightnessPercent(100.0, "Brightness should be set to 100.");

  // Manually set known_user's saved brightness to 10%.
  known_user.SetPath(account_id, prefs::kInternalDisplayScreenBrightnessPercent,
                     std::make_optional<base::Value>(10.0));
  EXPECT_EQ(GetBrightnessPrefValue(known_user, account_id), 10.0);

  // Simulate reboot.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  LoginScreenFocusAccount(account_id);

  // Expect the brightness is restored to 10%.
  ExpectBrightnessPercent(10, "Brightness should be set to 10.");
}

TEST_F(BrightnessControllerChromeosTest,
       RestoreBrightnessSettings_ScreenBrightnessPercentPolicyUnset) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableBrightnessControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);
  run_loop_.RunUntilIdle();

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Set brightness to 100%.
  SetAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
  SetBrightness(100.0,
                power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  ExpectBrightnessPercent(100.0, "Brightness should be set to 100.");

  // Manually set known_user's saved brightness to 10%.
  known_user.SetPath(account_id, prefs::kInternalDisplayScreenBrightnessPercent,
                     std::make_optional<base::Value>(10.0));
  EXPECT_EQ(GetBrightnessPrefValue(known_user, account_id), 10.0);

  // This time, set the brightness managed by policy to be true.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kPowerBatteryScreenBrightnessPercent,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kPowerBatteryScreenBrightnessPercent));

  // "Unplug" the device from charger
  SetBatteryPower();

  // Simulate reboot, and log in.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  SimulateUserLogin(kUserEmail);

  // Expect the brightness is not restored to 10%.
  brightness_control_delegate()->GetBrightnessPercent(
      base::BindLambdaForTesting([](std::optional<double> brightness_percent) {
        EXPECT_NE(brightness_percent.value(), 10.0);
      }));
}

TEST_F(BrightnessControllerChromeosTest,
       RecordStartupAmbientLightSensorStatus) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableBrightnessControlInSettings);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.Startup.AmbientLightSensorEnabled", 0);

  // Set ALS and sensor status.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_has_ambient_light_sensor(true);
  run_loop_.RunUntilIdle();

  // Log in.
  ClearLogin();
  AccountId first_account = AccountId::FromUserEmail(kUserEmail);
  LoginScreenFocusAccount(first_account);
  histogram_tester_->ExpectBucketCount(
      "ChromeOS.Display.Startup.AmbientLightSensorEnabled", true, 1);

  // Log in again, expect no extra metric is emitted.
  ClearLogin();
  LoginScreenFocusAccount(first_account);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Display.Startup.AmbientLightSensorEnabled", 1);
}

TEST_F(BrightnessControllerChromeosTest, RestoreBrightnessSettings_NoSensor) {
  // Test case: Disable ALS via brightness key and restore brightness settings.
  // When the device has no sensor. ALS should not be re-enabled after login.
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableBrightnessControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Disable ALS
  SetAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
  ExpectAmbientLightSensorEnabled(false, "ALS is disabled.");

  // Set the device to have no ambient light sensor.
  power_manager_client()->set_has_ambient_light_sensor(false);

  // Reinitialize controller to apply updates.
  auto controller = std::make_unique<system::BrightnessControllerChromeos>(
      local_state(), Shell::Get()->session_controller());
  run_loop_.RunUntilIdle();

  // Before reboot, set saved prefs: ALS disabled
  // reason (BRIGHTNESS_USER_REQUEST) and brightness percent (30.0).
  known_user.SetPath(
      account_id, prefs::kAmbientLightSensorDisabledReason,
      std::make_optional<base::Value>(
          power_manager::
              AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST));
  known_user.SetPath(account_id, prefs::kInternalDisplayScreenBrightnessPercent,
                     std::make_optional<base::Value>(30.0));

  // Simulate reboot, and log in again.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Verify ALS is not re-enabled, brightness percent is restored to 30.0.
  ExpectAmbientLightSensorEnabled(false, "ALS is not re-enabled.");
  ExpectBrightnessPercent(30.0, "brighntess is restored");
}

TEST_F(BrightnessControllerChromeosTest, RestoreBrightnessSettings_HasSensor) {
  // Test case: Disable ALS via brightness key and restore brightness settings.
  // When the device has a sensor. ALS should be re-enabled after login.
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableBrightnessControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetAmbientLightSensorEnabled(request);
  power_manager_client()->set_screen_brightness_percent(kInitialBrightness);

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Disable ALS
  SetAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
  ExpectAmbientLightSensorEnabled(false, "ALS is disabled");

  // Set the device to have ambient light sensor.
  power_manager_client()->set_has_ambient_light_sensor(true);

  // Reinitialize controller to apply updates.
  auto controller = std::make_unique<system::BrightnessControllerChromeos>(
      local_state(), Shell::Get()->session_controller());
  run_loop_.RunUntilIdle();

  // Before reboot, set saved prefs: ALS disabled
  // reason (BRIGHTNESS_USER_REQUEST) and brightness percent (30.0).
  known_user.SetPath(
      account_id, prefs::kAmbientLightSensorDisabledReason,
      std::make_optional<base::Value>(
          power_manager::
              AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST));
  known_user.SetPath(account_id, prefs::kAmbientLightSensorDisabledReason,
                     std::make_optional<base::Value>(30.0));

  // Simulate reboot, and log in again.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Verify ambient light sensor is re-enabled, because als was disabled by
  // brightness key, and the brightness percent should be
  // kInitialKeyboardBrightness instead of 30.0.
  ExpectAmbientLightSensorEnabled(true, "ALS is re-enabled.");
  ExpectBrightnessPercent(kInitialBrightness,
                          "Brightness percent should not be restored.");
}

}  // namespace ash
