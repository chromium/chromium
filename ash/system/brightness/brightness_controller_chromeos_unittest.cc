// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/brightness_controller_chromeos.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/shell.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/known_user.h"

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

  void AdvanceClock(base::TimeDelta time) {
    task_environment()->AdvanceClock(time);
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
  brightness_control_delegate()->SetAmbientLightSensorEnabled(false);
  // PowerManagerClient should have been invoked, disabling the ambient light
  // sensor.
  EXPECT_FALSE(power_manager_client()->is_ambient_light_sensor_enabled());

  // Re-enabled the ambient light sensor via the BrightnessControlDelegate.
  brightness_control_delegate()->SetAmbientLightSensorEnabled(true);
  // PowerManagerClient should have been invoked, re-enabling the ambient light
  // sensor.
  EXPECT_TRUE(power_manager_client()->is_ambient_light_sensor_enabled());
}

TEST_F(BrightnessControllerChromeosTest, GetAmbientLightSensorEnabled) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  SetBatteryPower();

  // Disable the ambient light sensor via the BrightnessControlDelegate.
  brightness_control_delegate()->SetAmbientLightSensorEnabled(false);

  // GetAmbientLightSensorEnabled should return that the the light sensor is
  // currently not enabled.
  brightness_control_delegate()->GetAmbientLightSensorEnabled(
      base::BindOnce([](std::optional<bool> is_ambient_light_sensor_enabled) {
        EXPECT_FALSE(is_ambient_light_sensor_enabled.value());
      }));

  // Re-enable the ambient light sensor via the BrightnessControlDelegate.
  brightness_control_delegate()->SetAmbientLightSensorEnabled(true);

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
  power_manager_client()->SetAmbientLightSensorEnabled(true);
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
  power_manager_client()->SetAmbientLightSensorEnabled(false);
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
  power_manager_client()->SetAmbientLightSensorEnabled(true);
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
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  SetBatteryPower();

  // Set the ambient light sensor to be enabled initially.
  power_manager_client()->SetAmbientLightSensorEnabled(true);
  // Wait for AmbientLightSensorEnabledChange observer to be notified.
  run_loop_.RunUntilIdle();

  // On the login screen, focus a user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  user_manager::KnownUser known_user(local_state());

  // There should not be a KnownUser pref set initially because a user account
  // wasn't focused at the time of the change.
  EXPECT_FALSE(
      HasDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));
  // However, the synced profile pref value should default to true.
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));

  // Disable the ambient light sensor.
  power_manager_client()->SetAmbientLightSensorEnabled(false);
  // Wait for AmbientLightSensorEnabledChange observer to be notified.
  run_loop_.RunUntilIdle();

  // After the ambient light sensor status is disabled, the KnownUser pref
  // should be stored with the correct value.
  EXPECT_TRUE(
      HasDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));
  EXPECT_FALSE(
      GetDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));
  // The synced profile pref should also have the correct value.
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kDisplayAmbientLightSensorLastEnabled));

  // Re-enable the ambient light sensor.
  power_manager_client()->SetAmbientLightSensorEnabled(true);
  // Wait for AmbientLightSensorEnabledChange observer to be notified.
  run_loop_.RunUntilIdle();

  // After the ambient light sensor status is re-enabled, the KnownUser pref
  // should be stored with the correct value.
  EXPECT_TRUE(
      HasDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));
  EXPECT_TRUE(
      GetDisplayAmbientLightSensorEnabledPrefValue(known_user, account_id));
  // The synced profile pref should also have the correct value.
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

}  // namespace ash
