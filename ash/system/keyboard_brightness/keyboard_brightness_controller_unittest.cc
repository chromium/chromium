// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/power/power_status.h"
#include "ash/test/ash_test_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/user_manager/known_user.h"

namespace ash {

namespace {

constexpr char kUserEmail[] = "user@example.com";
constexpr char kUserEmailSecondary[] = "user2@example.com";
constexpr double kInitialKeyboardBrightness = 40.0;

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

}  // namespace

class FakeKeyboardBrightnessControlDelegate
    : public KeyboardBrightnessControlDelegate {
 public:
  FakeKeyboardBrightnessControlDelegate() = default;
  ~FakeKeyboardBrightnessControlDelegate() override = default;

  // override methods:
  void HandleKeyboardBrightnessDown() override {}
  void HandleKeyboardBrightnessUp() override {}
  void HandleToggleKeyboardBacklight() override {}
  void HandleGetKeyboardBrightness(
      base::OnceCallback<void(std::optional<double>)> callback) override {
    std::move(callback).Run(keyboard_brightness_);
  }
  void HandleSetKeyboardBrightness(
      double percent,
      bool gradual,
      KeyboardBrightnessChangeSource source) override {
    keyboard_brightness_ = percent;
  }
  void HandleSetKeyboardAmbientLightSensorEnabled(
      bool enabled,
      KeyboardAmbientLightSensorEnabledChangeSource source) override {}

  void HandleGetKeyboardAmbientLightSensorEnabled(
      base::OnceCallback<void(std::optional<bool>)> callback) override {}

  double keyboard_brightness() { return keyboard_brightness_; }

  void OnReceiveHasKeyboardBacklight(
      std::optional<bool> has_keyboard_backlight) {
    if (has_keyboard_backlight.has_value()) {
      base::UmaHistogramBoolean("ChromeOS.Keyboard.HasBacklight",
                                has_keyboard_backlight.value());
    }
  }

  void OnReceiveHasAmbientLightSensor(std::optional<bool> has_sensor) {
    if (has_sensor.has_value()) {
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Device.HasAmbientLightSensor", has_sensor.value());
    }
  }

 private:
  double keyboard_brightness_ = 0;
};

class KeyboardBrightnessControllerTest : public AshTestBase {
 public:
  KeyboardBrightnessControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    delegate_ = std::make_unique<FakeKeyboardBrightnessControlDelegate>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    delegate_.reset();
  }

  KeyboardBrightnessControlDelegate* keyboard_brightness_control_delegate() {
    return Shell::Get()->keyboard_brightness_control_delegate();
  }

  LoginDataDispatcher* login_data_dispatcher() {
    return Shell::Get()->login_screen_controller()->data_dispatcher();
  }

  // Whether prefs::kKeyboardAmbientLightSensorEnabled has been set.
  bool HasKeyboardAmbientLightSensorEnabledPrefValue(
      const user_manager::KnownUser& known_user,
      const AccountId& account_id) {
    const base::Value* pref_value = known_user.FindPath(
        account_id, prefs::kKeyboardAmbientLightSensorEnabled);
    if (!pref_value) {
      return false;
    }

    return pref_value->is_bool();
  }

  // Gets the KnownUser keyboard ALS enabled pref value. Only call this
  // function if HasKeyboardAmbientLightSensorPrefValue is true.
  bool GetKeyboardAmbientLightSensorEnabledPrefValue(
      const user_manager::KnownUser& known_user,
      const AccountId& account_id) {
    return known_user
        .FindPath(account_id, prefs::kKeyboardAmbientLightSensorEnabled)
        ->GetBool();
  }

  // Whether prefs::kKeyboardBrightnessPercent has been set.
  bool HasKeyboardBrightnessPrefValue(const user_manager::KnownUser& known_user,
                                      const AccountId& account_id) {
    const base::Value* pref_value =
        known_user.FindPath(account_id, prefs::kKeyboardBrightnessPercent);
    if (!pref_value) {
      return false;
    }

    return pref_value->is_double();
  }

  // Gets the KnownUser keyboard brightness percent pref value. Only call this
  // function if HasKeyboardBrightnessPrefValue is true.
  double GetKeyboardBrightnessPrefValue(
      const user_manager::KnownUser& known_user,
      const AccountId& account_id) {
    return known_user.FindPath(account_id, prefs::kKeyboardBrightnessPercent)
        ->GetDouble();
  }

  // Return true if there is a KnownUser pref for the keyboard ambient light
  // sensor disabled reason.
  bool HasKeyboardAmbientLightSensorDisabledReasonPrefValue(
      const user_manager::KnownUser& known_user,
      const AccountId& account_id) {
    const base::Value* pref_value = known_user.FindPath(
        account_id, prefs::kKeyboardAmbientLightSensorDisabledReason);
    if (!pref_value) {
      return false;
    }

    return pref_value->is_int();
  }

  // Gets the KnownUser keyboard ambient light sensor disabled reason pref
  // value. Only call this function if
  // HasKeyboardAmbientLightSensorDisabledReasonPrefValue is true.
  int GetKeyboardAmbientLightSensorDisabledReasonPrefValue(
      const user_manager::KnownUser& known_user,
      const AccountId& account_id) {
    return known_user
        .FindPath(account_id, prefs::kKeyboardAmbientLightSensorDisabledReason)
        ->GetInt();
  }

  void SetKeyboardAmbientLightSensorEnabled(
      bool enabled,
      power_manager::AmbientLightSensorChange_Cause cause) {
    keyboard_brightness_control_delegate()
        ->HandleSetKeyboardAmbientLightSensorEnabled(
            enabled,
            KeyboardAmbientLightSensorEnabledChangeSource::kSettingsApp);
    power_manager::AmbientLightSensorChange sensor_change;
    sensor_change.set_sensor_enabled(enabled);
    sensor_change.set_cause(cause);
    power_manager_client()->SendKeyboardAmbientLightSensorEnabledChanged(
        sensor_change);
  }

  void SetKeyboardBrightness(
      double keyboard_brightness,
      power_manager::BacklightBrightnessChange_Cause cause) {
    keyboard_brightness_control_delegate()->HandleSetKeyboardBrightness(
        keyboard_brightness, /*gradual=*/false,
        KeyboardBrightnessChangeSource::kSettingsApp);
    power_manager::BacklightBrightnessChange brightness_change;
    brightness_change.set_percent(keyboard_brightness);
    brightness_change.set_cause(cause);
    power_manager_client()->set_keyboard_brightness_percent(
        keyboard_brightness);
    power_manager_client()->SendKeyboardBrightnessChanged(brightness_change);
  }

  // On the login screen, focus the given account.
  void LoginScreenFocusAccount(const AccountId account_id) {
    login_data_dispatcher()->NotifyFocusPod(account_id);
    run_loop_.RunUntilIdle();
  }

  // Check if keyboard ambient light sensor status is equal to expected value.
  void ExpectKeyboardAmbientLightSensorEnabled(bool expected_value) {
    keyboard_brightness_control_delegate()
        ->HandleGetKeyboardAmbientLightSensorEnabled(base::BindLambdaForTesting(
            [expected_value](std::optional<bool> sensor_enabled) {
              EXPECT_EQ(sensor_enabled.value(), expected_value);
            }));
  }

  // Check if keyboard brightness is equal to expected value.
  void ExpectKeyboardBrightnessPercent(double expected_value) {
    keyboard_brightness_control_delegate()->HandleGetKeyboardBrightness(
        base::BindLambdaForTesting(
            [expected_value](std::optional<double> keyboard_brightness) {
              EXPECT_EQ(keyboard_brightness.value(), expected_value);
            }));
  }

  void AdvanceClock(base::TimeDelta time) {
    task_environment()->AdvanceClock(time);
  }

 protected:
  base::RunLoop run_loop_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<FakeKeyboardBrightnessControlDelegate> delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(KeyboardBrightnessControllerTest, RecordHasKeyboardBrightness) {
  histogram_tester_->ExpectTotalCount("ChromeOS.Keyboard.HasBacklight", 0);
  delegate_->OnReceiveHasKeyboardBacklight(std::optional<bool>(true));
  histogram_tester_->ExpectTotalCount("ChromeOS.Keyboard.HasBacklight", 1);
}

TEST_F(KeyboardBrightnessControllerTest, RecordHasAmbientLightSensor) {
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Settings.Device.HasAmbientLightSensor", 0);
  delegate_->OnReceiveHasAmbientLightSensor(std::optional<bool>(true));
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Settings.Device.HasAmbientLightSensor", 1);
}

TEST_F(KeyboardBrightnessControllerTest, SetKeyboardAmbientLightSensorEnabled) {
  // Ambient light sensor is enabled by default.
  EXPECT_TRUE(power_manager_client()->keyboard_ambient_light_sensor_enabled());
  // Disable the ambient light sensor.
  keyboard_brightness_control_delegate()
      ->HandleSetKeyboardAmbientLightSensorEnabled(
          false, KeyboardAmbientLightSensorEnabledChangeSource::kSettingsApp);

  // Verify that the ambient light sensor is now disabled.
  EXPECT_FALSE(power_manager_client()->keyboard_ambient_light_sensor_enabled());

  keyboard_brightness_control_delegate()
      ->HandleGetKeyboardAmbientLightSensorEnabled(base::BindOnce(
          [](std::optional<bool> is_ambient_light_sensor_enabled) {
            EXPECT_FALSE(is_ambient_light_sensor_enabled.value());
          }));

  // Re-enabled the ambient light sensor
  keyboard_brightness_control_delegate()
      ->HandleSetKeyboardAmbientLightSensorEnabled(
          true, KeyboardAmbientLightSensorEnabledChangeSource::kSettingsApp);

  // Verify that the ambient light sensor is enabled.
  EXPECT_TRUE(power_manager_client()->keyboard_ambient_light_sensor_enabled());

  keyboard_brightness_control_delegate()
      ->HandleGetKeyboardAmbientLightSensorEnabled(base::BindOnce(
          [](std::optional<bool> is_ambient_light_sensor_enabled) {
            EXPECT_TRUE(is_ambient_light_sensor_enabled.value());
          }));
}

TEST_F(KeyboardBrightnessControllerTest, SaveKeyboardALSPrefToKnownUser) {
  // Set initial ALS status.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  AccountId account_id = AccountId::FromUserEmail(kUserEmail);

  // Create a KnownUser for this Local State.
  user_manager::KnownUser known_user(local_state());

  // On the login screen, focus the user.
  LoginScreenFocusAccount(account_id);

  EXPECT_FALSE(
      HasKeyboardAmbientLightSensorEnabledPrefValue(known_user, account_id));

  // Set the Keyboard ALS enabled to false for the user.
  SetKeyboardAmbientLightSensorEnabled(
      false, power_manager::AmbientLightSensorChange_Cause::
                 AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);

  // Expect pref value is saved, and the pref value is set to false.
  EXPECT_TRUE(
      HasKeyboardAmbientLightSensorEnabledPrefValue(known_user, account_id));
  EXPECT_FALSE(
      GetKeyboardAmbientLightSensorEnabledPrefValue(known_user, account_id));
}

TEST_F(KeyboardBrightnessControllerTest, SaveBrightnessPrefToKnownUserOnLogin) {
  // Set initial brightness.
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // Create a KnownUser for this Local State.
  user_manager::KnownUser known_user(local_state());

  // On the login screen, focus the user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  LoginScreenFocusAccount(account_id);
  EXPECT_FALSE(
      HasKeyboardAmbientLightSensorEnabledPrefValue(known_user, account_id));

  // Simulate the brightness changing automatically due to inactivity.
  // This should not be saved into Local State.
  SetKeyboardBrightness(
      0.0, power_manager::BacklightBrightnessChange_Cause_USER_INACTIVITY);
  EXPECT_FALSE(HasKeyboardBrightnessPrefValue(known_user, account_id));

  // Simulate the brightness changing automatically due to user activity.
  // This should not be saved into Local State.
  SetKeyboardBrightness(
      60.0, power_manager::BacklightBrightnessChange_Cause_USER_ACTIVITY);
  EXPECT_FALSE(HasKeyboardBrightnessPrefValue(known_user, account_id));

  // Set the brightness to a new value via user action.
  double brightness_change_percent = 12.0;
  SetKeyboardBrightness(
      brightness_change_percent,
      power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);

  // Expect pref value is saved, and the pref value is set to
  // brightness_change_percent.
  EXPECT_TRUE(HasKeyboardBrightnessPrefValue(known_user, account_id));
  EXPECT_EQ(GetKeyboardBrightnessPrefValue(known_user, account_id),
            brightness_change_percent);
}

TEST_F(KeyboardBrightnessControllerTest,
       SaveBrightnessPrefToKnownUserAfterLogin) {
  // Set initial brightness.
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // Create a KnownUser for this Local State.
  user_manager::KnownUser known_user(local_state());

  // On the login screen, focus the user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  LoginScreenFocusAccount(account_id);
  EXPECT_FALSE(HasKeyboardBrightnessPrefValue(known_user, account_id));

  // Simulate user login.
  SimulateUserLogin(kUserEmail);
  run_loop_.RunUntilIdle();

  // After login, the brightness pref should have a value equal to the initial
  // brightness level.
  EXPECT_TRUE(HasKeyboardBrightnessPrefValue(known_user, account_id));
  EXPECT_EQ(GetKeyboardBrightnessPrefValue(known_user, account_id),
            kInitialKeyboardBrightness);

  // Simulate the keyboard brightness changing automatically due to inactivity.
  // This should not be saved into Local State.
  SetKeyboardBrightness(
      0.0, power_manager::BacklightBrightnessChange_Cause_USER_INACTIVITY);
  EXPECT_EQ(GetKeyboardBrightnessPrefValue(known_user, account_id),
            kInitialKeyboardBrightness);

  // Change the brightness via user request (this time, after login).
  double new_brightness_change_percent = 96.0;
  SetKeyboardBrightness(
      new_brightness_change_percent,
      power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);

  // The Local State brightness pref should have the new brightness value
  // stored.
  EXPECT_TRUE(HasKeyboardBrightnessPrefValue(known_user, account_id));
  EXPECT_EQ(GetKeyboardBrightnessPrefValue(known_user, account_id),
            new_brightness_change_percent);

  // Change the brightness via user request, from the Settings app.
  double settings_brightness_change_percent = 22.2;
  SetKeyboardBrightness(
      settings_brightness_change_percent,
      power_manager::
          BacklightBrightnessChange_Cause_USER_REQUEST_FROM_SETTINGS_APP);

  // The Local State brightness pref should have the new brightness value
  // stored.
  EXPECT_EQ(GetKeyboardBrightnessPrefValue(known_user, account_id),
            settings_brightness_change_percent);
}

TEST_F(KeyboardBrightnessControllerTest, SavePrefToKnownUserMultipleUser) {
  // Set initial brightness.
  power_manager_client()->set_screen_brightness_percent(
      kInitialKeyboardBrightness);

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  AccountId first_account = AccountId::FromUserEmail(kUserEmail);
  AccountId second_account = AccountId::FromUserEmail(kUserEmailSecondary);

  // On the login screen, focus the first user.
  LoginScreenFocusAccount(first_account);

  // Create a KnownUser for this Local State.
  user_manager::KnownUser known_user(local_state());

  EXPECT_FALSE(HasKeyboardBrightnessPrefValue(known_user, first_account));
  EXPECT_FALSE(HasKeyboardBrightnessPrefValue(known_user, second_account));

  // Set the brightness to a new value via user action.
  double first_brightness_change_percent = 12.0;
  SetKeyboardBrightness(
      first_brightness_change_percent,
      power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);

  // The brightness Local State pref should now be set, with the value of the
  // current brightness, the second user should still not have a pref set.
  EXPECT_EQ(GetKeyboardBrightnessPrefValue(known_user, first_account),
            first_brightness_change_percent);
  EXPECT_FALSE(HasKeyboardBrightnessPrefValue(known_user, second_account));

  // On the login screen, focus the second user.
  LoginScreenFocusAccount(second_account);

  // Set the brightness to a new value via user action for the second user.
  double second_brightness_change_percent = 99.9;
  SetKeyboardBrightness(
      second_brightness_change_percent,
      power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);

  // The second user have the pref value set to
  // second_brightness_change_percent.
  EXPECT_EQ(GetKeyboardBrightnessPrefValue(known_user, second_account),
            second_brightness_change_percent);

  // The first user still have the old brightness percent value.
  EXPECT_EQ(GetKeyboardBrightnessPrefValue(known_user, first_account),
            first_brightness_change_percent);
}

TEST_F(KeyboardBrightnessControllerTest,
       RestoreKeyboardBrightnessSettings_FlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, focus the first user.
  AccountId first_account = AccountId::FromUserEmail(kUserEmail);
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);
  ExpectKeyboardBrightnessPercent(kInitialKeyboardBrightness);

  // Then, focus the second user.
  AccountId second_account = AccountId::FromUserEmail(kUserEmailSecondary);
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);
  ExpectKeyboardBrightnessPercent(kInitialKeyboardBrightness);

  // Switch back to the first user, then disable ALS by changing the brightness.
  LoginScreenFocusAccount(first_account);
  const double first_brightness_change_percent = 20.0;
  keyboard_brightness_control_delegate()->HandleSetKeyboardBrightness(
      first_brightness_change_percent, /*gradual=*/false,
      KeyboardBrightnessChangeSource::kSettingsApp);

  // ALS should be disabled for the first user.
  run_loop_.RunUntilIdle();
  ExpectKeyboardAmbientLightSensorEnabled(false);
  ExpectKeyboardBrightnessPercent(first_brightness_change_percent);

  // ALS should remain enabled for the second user, despite being disabled for
  // the first user, Brightness should remain the same after switching to the
  // second user.
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);
  ExpectKeyboardBrightnessPercent(first_brightness_change_percent);

  // ALS should be disabled for first user after switching back from second
  // user.
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(false);
  ExpectKeyboardBrightnessPercent(first_brightness_change_percent);

  // Simulate a reboot, which resets the value of the keyboard ambient light
  // sensor and the keyboard brightness.
  ClearLogin();
  power_manager::SetAmbientLightSensorEnabledRequest request2;
  request2.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request2);
  power_manager_client()->set_screen_brightness_percent(
      kInitialKeyboardBrightness);

  // After reboot, ALS should be still be disabled for the first user, and
  // keyboard brightness is restored.
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(false);
  ExpectKeyboardBrightnessPercent(first_brightness_change_percent);

  // After reboot, ALS should be still be enabled for second user, and keyboard
  // brightness should be should be equal to the last value set (since
  // auto-brightness is enabled).
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);
  ExpectKeyboardBrightnessPercent(first_brightness_change_percent);

  // Switch back to the first user, ALS should remain disabled.
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(false);
  ExpectKeyboardBrightnessPercent(first_brightness_change_percent);
}

TEST_F(KeyboardBrightnessControllerTest,
       RestoreKeyboardBrightnessSettings_FlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Set initial ALS status.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, focus the first user.
  AccountId first_account = AccountId::FromUserEmail(kUserEmail);
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);
  ExpectKeyboardBrightnessPercent(kInitialKeyboardBrightness);

  // Then, focus the second user.
  AccountId second_account = AccountId::FromUserEmail(kUserEmailSecondary);
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);
  ExpectKeyboardBrightnessPercent(kInitialKeyboardBrightness);

  // Switch back to the first user, then disable ALS by changing the brightness.
  LoginScreenFocusAccount(first_account);
  const double first_brightness_change_percent = 20.0;
  keyboard_brightness_control_delegate()->HandleSetKeyboardBrightness(
      first_brightness_change_percent, /*gradual=*/false,
      KeyboardBrightnessChangeSource::kSettingsApp);

  // ALS should be disabled for the first user.
  run_loop_.RunUntilIdle();
  ExpectKeyboardAmbientLightSensorEnabled(false);
  ExpectKeyboardBrightnessPercent(first_brightness_change_percent);

  // ALS should be disabled for second user because the ALS value is not being
  // restored from prefs.
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(false);
  ExpectKeyboardBrightnessPercent(first_brightness_change_percent);

  // ALS should be disabled for first user after switching back from second
  // user.
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(false);

  // Simulate a reboot, which resets the value of the ambient light sensor.
  // the keyboard brightness.
  ClearLogin();
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);

  // After reboot, ALS should be enabled for the first user by default, the
  // brightness level should be equal to the initial brightness for the first
  // user.
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);
  ExpectKeyboardBrightnessPercent(kInitialKeyboardBrightness);

  // After reboot, ALS should be enabled for the second user by default, and
  // brightness level is default level.
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);
  ExpectKeyboardBrightnessPercent(kInitialKeyboardBrightness);

  // Switch back to the first user, settings should remain the same.
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);
  ExpectKeyboardBrightnessPercent(kInitialKeyboardBrightness);
}

TEST_F(KeyboardBrightnessControllerTest, KeyboardALSDisabledReasonPref) {
  // Set initial ALS status and brightness level.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);

  // On the login screen, focus a user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  user_manager::KnownUser known_user(local_state());

  // Confirm that no "disabled reason" pref exists for the given KnownUser.
  EXPECT_FALSE(HasKeyboardAmbientLightSensorDisabledReasonPrefValue(
      known_user, account_id));

  // Disable the keyboard ambient light sensor.
  SetKeyboardAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);

  // There should now be a "disabled reason" pref stored in KnownUser.
  EXPECT_TRUE(HasKeyboardAmbientLightSensorDisabledReasonPrefValue(known_user,
                                                                   account_id));
  EXPECT_EQ(
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP,
      GetKeyboardAmbientLightSensorDisabledReasonPrefValue(known_user,
                                                           account_id));

  // Re-enable the keyboard ambient light sensor.
  SetKeyboardAmbientLightSensorEnabled(
      true,
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);

  // After the keyboard ambient light sensor is re-enabled, the "disabled
  // reason" pref should be deleted.
  EXPECT_FALSE(HasKeyboardAmbientLightSensorDisabledReasonPrefValue(
      known_user, account_id));

  // Test with other causes.
  SetKeyboardAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
  EXPECT_TRUE(HasKeyboardAmbientLightSensorDisabledReasonPrefValue(known_user,
                                                                   account_id));
  EXPECT_EQ(
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST,
      GetKeyboardAmbientLightSensorDisabledReasonPrefValue(known_user,
                                                           account_id));
}

TEST_F(KeyboardBrightnessControllerTest, KeyboardAmbientLightEnabledUserPref) {
  // Activate user session.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Set the ambient light sensor to be enabled initially.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  run_loop_.RunUntilIdle();

  // User pref is default to true.
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));

  // Disable the sensor via brightness change (not from settings app), pref
  // should remain true.
  SetKeyboardAmbientLightSensorEnabled(
      false, power_manager::AmbientLightSensorChange_Cause::
                 AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));

  // Re-enable the sensor from settings app, pref should be true.
  SetKeyboardAmbientLightSensorEnabled(
      true, power_manager::AmbientLightSensorChange_Cause::
                AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));

  // Disable the sensor again, this time, the request is from settings app, the
  // pref should be updated to false.
  SetKeyboardAmbientLightSensorEnabled(
      false, power_manager::AmbientLightSensorChange_Cause::
                 AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));

  // Re-enable the sensor via user settings and verify the preference updates.
  SetKeyboardAmbientLightSensorEnabled(
      true, power_manager::AmbientLightSensorChange_Cause::
                AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));
}

TEST_F(KeyboardBrightnessControllerTest,
       RestoreKeyboardALSSettingForNewUser_FlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Set initial ALS and keyboard brightness.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);
  run_loop_.RunUntilIdle();

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, select and login with an existing user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  login_data_dispatcher()->NotifyFocusPod(account_id);
  LoginScreenFocusAccount(account_id);
  SimulateUserLogin(kUserEmail);

  // The ambient light sensor should be enabled by default.
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // Set Keyboard ALS status to false.
  SetKeyboardAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);

  // The synced profile pref should have the correct value (false).
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));
  ExpectKeyboardAmbientLightSensorEnabled(false);

  // Simulate a reboot, which resets the value of the ambient light sensor and
  // the keyboard brightness.
  ClearLogin();
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);

  // Simulate a login with a second user.
  SimulateNewUserFirstLogin(kUserEmailSecondary);

  // Verify default keyboard brightness settings.
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // Manually set the user pref, which will be synced to the first user later.
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kKeyboardAmbientLightSensorLastEnabled, false);

  // Now, login the first user again, as if it's that user's first time
  // logging in on this device.
  SimulateNewUserFirstLogin(kUserEmail);

  // The value of the synced profile pref for the keyboard ambient light sensor
  // should be false, because on the "other device" that value was set to false.
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));
  ExpectKeyboardAmbientLightSensorEnabled(false);
}

TEST_F(KeyboardBrightnessControllerTest,
       RestoreKeyboardALSSettingForNewUser_FlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Set initial ALS and keyboard brightness.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);
  run_loop_.RunUntilIdle();

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, select and login with an existing user.
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  login_data_dispatcher()->NotifyFocusPod(account_id);
  LoginScreenFocusAccount(account_id);
  SimulateUserLogin(kUserEmail);

  // The ambient light sensor should be enabled by default.
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // Set Keyboard ALS status to false.
  SetKeyboardAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);

  // The synced profile pref should have the correct value (false).
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));
  ExpectKeyboardAmbientLightSensorEnabled(false);

  // Simulate a reboot, which resets the value of the ambient light sensor and
  // the keyboard brightness.
  ClearLogin();
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);

  // Simulate a login with a second user.
  SimulateNewUserFirstLogin(kUserEmailSecondary);

  // Verify default keyboard brightness settings.
  EXPECT_TRUE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // Manually set the user pref, which will be synced to the first user later.
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kKeyboardAmbientLightSensorLastEnabled, false);

  // Now, login the first user again, as if it's that user's first time
  // logging in on this device.
  SimulateNewUserFirstLogin(kUserEmail);

  // The value of the synced profile pref for the keyboard ambient light sensor
  // should be false, because on the "other device" that value was set to false.
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
          prefs::kKeyboardAmbientLightSensorLastEnabled));

  // However, because the flag is disabled, the keyboard  ambient light sensor
  // preference will not be restored, and thus the keyboard ambient light sensor
  // should be disabled.
  ExpectKeyboardAmbientLightSensorEnabled(true);
}

TEST_F(KeyboardBrightnessControllerTest, SetKeyboardBrightness_Cause) {
  // Keyboard brightness changes from Quick Settings should have cause
  // "USER_REQUEST".
  keyboard_brightness_control_delegate()->HandleSetKeyboardBrightness(
      50, /*gradual=*/true, KeyboardBrightnessChangeSource::kQuickSettings);
  EXPECT_EQ(power_manager_client()->requested_keyboard_brightness_cause(),
            power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);

  // Keyboard brightness changes from the Settings app should have cause
  // "USER_REQUEST_FROM_SETTINGS_APP".
  keyboard_brightness_control_delegate()->HandleSetKeyboardBrightness(
      50, /*gradual=*/true, /*source=*/
      KeyboardBrightnessChangeSource::kSettingsApp);
  EXPECT_EQ(
      power_manager_client()->requested_keyboard_brightness_cause(),
      power_manager::
          SetBacklightBrightnessRequest_Cause_USER_REQUEST_FROM_SETTINGS_APP);
}

TEST_F(KeyboardBrightnessControllerTest,
       ReenableKeyboardAmbientLightSensor_Reboot_DisabledFromSettingsApp) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Set initial ALS and keyboard brightness.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);
  run_loop_.RunUntilIdle();

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Set ALS to false, and set the disabled reason to be
  // USER_REQUEST_SETTINGS_APP.
  SetKeyboardAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
  known_user.SetPath(
      account_id, prefs::kKeyboardAmbientLightSensorDisabledReason,
      std::make_optional<base::Value>(
          power_manager::
              AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP));

  // ALS is disabled.
  ExpectKeyboardAmbientLightSensorEnabled(false);
  EXPECT_EQ(false, GetKeyboardAmbientLightSensorEnabledPrefValue(known_user,
                                                                 account_id));

  // "disabled reason" pref stored in KnownUser should be
  // USER_REQUEST_SETTINGS_APP.
  EXPECT_TRUE(HasKeyboardAmbientLightSensorDisabledReasonPrefValue(known_user,
                                                                   account_id));
  EXPECT_EQ(
      power_manager::AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP,
      GetKeyboardAmbientLightSensorDisabledReasonPrefValue(known_user,
                                                           account_id));

  // Simulate reboot, and log in again.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  known_user.SetPath(account_id, prefs::kKeyboardBrightnessPercent,
                     std::make_optional<base::Value>(30.0));
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Expect ambient light sensor remain disabled, and brightness should be
  // restored.
  ExpectKeyboardAmbientLightSensorEnabled(false);
  ExpectKeyboardBrightnessPercent(30.0);

  // Simulate reboot, and log in the third time.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // ALS and brightness should remain the same as last reboot.
  ExpectKeyboardAmbientLightSensorEnabled(false);
  ExpectKeyboardBrightnessPercent(30.0);
}

TEST_F(KeyboardBrightnessControllerTest,
       ReenableKeyboardAmbientLightSensor_Reboot_DisabledFromBrightnessKey) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Set initial ALS and keyboard brightness.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);
  run_loop_.RunUntilIdle();

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Set ALS to false by brightness key, and set the disabled reason to be
  // BRIGHTNESS_USER_REQUEST.
  SetKeyboardAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
  known_user.SetPath(
      account_id, prefs::kKeyboardAmbientLightSensorDisabledReason,
      std::make_optional<base::Value>(
          power_manager::
              AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST));

  // ALS is disabled.
  ExpectKeyboardAmbientLightSensorEnabled(false);
  EXPECT_EQ(false, GetKeyboardAmbientLightSensorEnabledPrefValue(known_user,
                                                                 account_id));

  // "disabled reason" pref stored in KnownUser should be
  // USER_REQUEST_SETTINGS_APP.
  EXPECT_TRUE(HasKeyboardAmbientLightSensorDisabledReasonPrefValue(known_user,
                                                                   account_id));
  EXPECT_EQ(
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST,
      GetKeyboardAmbientLightSensorDisabledReasonPrefValue(known_user,
                                                           account_id));

  // Simulate reboot, and log in again.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  known_user.SetPath(account_id, prefs::kKeyboardBrightnessPercent,
                     std::make_optional<base::Value>(30.0));
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Expect ambient light sensor is re-enabled.
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // Simulate reboot, and log in the third time.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // ALS and should remain the same as last reboot.
  ExpectKeyboardAmbientLightSensorEnabled(true);
}

TEST_F(KeyboardBrightnessControllerTest,
       BrightnessSettingsUnchanged_DeviceLocked) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Set initial ALS and keyboard brightness.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);
  run_loop_.RunUntilIdle();

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Disable ALS using the brightness key.
  SetKeyboardAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);

  // Current status: Als is turned off, and current brightness is
  // kInitialBrightness.
  ExpectKeyboardAmbientLightSensorEnabled(false);
  ExpectKeyboardBrightnessPercent(kInitialKeyboardBrightness);

  // Simulate device lock and re-login.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Als should not be re-enabled, although it was not previously disabled from
  // settings app. The brightness percent should still be
  // kInitialKeyboardBrightness.
  ExpectKeyboardAmbientLightSensorEnabled(false);
  ExpectKeyboardBrightnessPercent(kInitialKeyboardBrightness);
}

TEST_F(KeyboardBrightnessControllerTest, RestoreBrightnessSettings_NoSensor) {
  // Test case: Disable ALS via brightness key and restore brightness settings.
  // When the device has no sensor. ALS should not be re-enabled after login.
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Set initial ALS and keyboard brightness.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);
  run_loop_.RunUntilIdle();

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Disable ALS
  SetKeyboardAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
  ExpectKeyboardAmbientLightSensorEnabled(false);

  // Set the device to have no ambient light sensor.
  power_manager_client()->set_has_ambient_light_sensor(false);
  power_manager_client()->set_has_keyboard_backlight(true);

  // Reinitialize controller to apply updates.
  auto controller = std::make_unique<KeyboardBrightnessController>(
      local_state(), Shell::Get()->session_controller());
  run_loop_.RunUntilIdle();

  // Before reboot, set saved prefs: ALS disabled
  // reason (BRIGHTNESS_USER_REQUEST) and brightness percent (30.0).
  known_user.SetPath(
      account_id, prefs::kKeyboardAmbientLightSensorDisabledReason,
      std::make_optional<base::Value>(
          power_manager::
              AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST));
  known_user.SetPath(account_id, prefs::kKeyboardBrightnessPercent,
                     std::make_optional<base::Value>(30.0));

  // Simulate reboot, and log in again.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Verify ALS is not re-enabled, brightness percent is restored to 30.0.
  ExpectKeyboardAmbientLightSensorEnabled(false);
  ExpectKeyboardBrightnessPercent(30.0);
}

TEST_F(KeyboardBrightnessControllerTest, RestoreBrightnessSettings_HasSensor) {
  // Test case: Disable ALS via brightness key and restore brightness settings.
  // when the device has a sensor. ALS should be re-enabled after login.
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Set initial ALS and keyboard brightness.
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_keyboard_brightness_percent(
      kInitialKeyboardBrightness);
  run_loop_.RunUntilIdle();

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Set ALS to false.
  SetKeyboardAmbientLightSensorEnabled(
      false,
      power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
  ExpectKeyboardAmbientLightSensorEnabled(false);

  // Set the device to have an ambient light sensor.
  power_manager_client()->set_has_ambient_light_sensor(true);
  power_manager_client()->set_has_keyboard_backlight(true);

  // Reinitialize controller to apply updates.
  auto controller = std::make_unique<KeyboardBrightnessController>(
      local_state(), Shell::Get()->session_controller());
  run_loop_.RunUntilIdle();

  // Before reboot, set saved prefs: ALS disabled
  // reason(BRIGHTNESS_USER_REQUEST) and brightness percent (30.0).
  known_user.SetPath(
      account_id, prefs::kKeyboardAmbientLightSensorDisabledReason,
      std::make_optional<base::Value>(
          power_manager::
              AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST));
  known_user.SetPath(account_id, prefs::kKeyboardBrightnessPercent,
                     std::make_optional<base::Value>(30.0));

  // Simulate reboot and login again.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  LoginScreenFocusAccount(account_id);

  // Verify ambient light sensor is re-enabled, because als was disabled by
  // brightness key, and the brightness percent should be
  // kInitialKeyboardBrightness instead of 30.0.
  ExpectKeyboardAmbientLightSensorEnabled(true);
  ExpectKeyboardBrightnessPercent(kInitialKeyboardBrightness);
}

TEST_F(KeyboardBrightnessControllerTest,
       RecordStartupKeyboardAmbientLightSensorStatus) {
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.Startup.AmbientLightSensorEnabled", 0);
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableKeyboardBacklightControlInSettings);
  power_manager::SetAmbientLightSensorEnabledRequest request;
  request.set_sensor_enabled(true);
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(request);
  power_manager_client()->set_has_ambient_light_sensor(true);
  base::RunLoop().RunUntilIdle();

  // Log in.
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  LoginScreenFocusAccount(account_id);
  histogram_tester_->ExpectBucketCount(
      "ChromeOS.Keyboard.Startup.AmbientLightSensorEnabled", true, 1);

  // Log in again, expect no extra metric is emitted.
  ClearLogin();
  LoginScreenFocusAccount(account_id);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.Startup.AmbientLightSensorEnabled", 1);
}

TEST_F(KeyboardBrightnessControllerTest,
       HistogramTest_DecreaseBrightnessOnLoginScreen) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLoginScreen."
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
  keyboard_brightness_control_delegate()->HandleKeyboardBrightnessDown();

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      base::Seconds(seconds_to_wait), 1);

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for a period of time, then send another brightness event
  AdvanceClock(base::Seconds(5));
  keyboard_brightness_control_delegate()->HandleKeyboardBrightnessDown();

  // The number of events should not have changed, since we already recorded a
  // metric on the login screen.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
}

TEST_F(KeyboardBrightnessControllerTest, HistogramTest_LoginSecondary) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLoginScreen."
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
  keyboard_brightness_control_delegate()->HandleKeyboardBrightnessDown();

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      base::Seconds(seconds_to_wait), 1);

  // Login
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Wait for a period of time, then send another brightness event
  AdvanceClock(base::Seconds(5));
  keyboard_brightness_control_delegate()->HandleKeyboardBrightnessDown();

  // The number of events should not have changed, since we already recorded a
  // metric on the login screen.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "DecreaseBrightness.BatteryPower",
      1);
}

TEST_F(KeyboardBrightnessControllerTest, HistogramTest_PowerSourceCharger) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLoginScreen."
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
  keyboard_brightness_control_delegate()->HandleKeyboardBrightnessUp();

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "IncreaseBrightness.ChargerPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLoginScreen."
      "IncreaseBrightness.ChargerPower",
      base::Seconds(seconds_to_wait), 1);
}

TEST_F(KeyboardBrightnessControllerTest,
       HistogramTest_BrightnessChangeAfterLogin) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.AfterLogin."
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
  keyboard_brightness_control_delegate()->HandleSetKeyboardBrightness(
      50, true, KeyboardBrightnessChangeSource::kSettingsApp);

  // Expect a record with the number of seconds since login, not since the
  // beginning of the login screen.
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.AfterLogin."
      "SetBrightness.ChargerPower",
      base::Seconds(seconds_to_wait), 1);
}

TEST_F(KeyboardBrightnessControllerTest,
       HistogramTest_BrightnessChangeAfterLogin_LongDuration) {
  // Metric count should start at 0.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.AfterLogin."
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
  keyboard_brightness_control_delegate()->HandleKeyboardBrightnessUp();

  // Expect a record with the number of minutes since login, not since the
  // beginning of the login screen.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.BatteryPower",
      1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.BatteryPower",
      base::Minutes(minutes_to_wait), 1);
}

TEST_F(KeyboardBrightnessControllerTest,
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
  keyboard_brightness_control_delegate()->HandleKeyboardBrightnessUp();

  // Since the brightness event occurred >1 hour after login, don't record it.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.AfterLogin."
      "IncreaseBrightness.BatteryPower",
      0);
}

TEST_F(KeyboardBrightnessControllerTest,
       HistogramTest_SetBrightnessAfterSystemRestoration) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Start on the login screen
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  SetBatteryPower();

  // Metrics count should start at 0, both OnLogin and AfterLogin.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLogin."
      "SetBrightness.BatteryPower",
      0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.AfterLogin."
      "SetBrightness.BatteryPower",
      0);

  // Log in
  ClearLogin();
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager::KnownUser known_user(local_state());
  SimulateUserLogin(kUserEmail);

  // Set keyboard brightness.
  known_user.SetPath(account_id, prefs::kKeyboardBrightnessPercent,
                     std::make_optional<base::Value>(30.0));

  // Simulate reboot, brightness should be restored.
  login_data_dispatcher()->NotifyFocusPod(account_id);

  // Verify that system restoring brightness is not recorded.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.OnLogin."
      "SetBrightness.BatteryPower",
      0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.AfterLogin."
      "SetBrightness.BatteryPower",
      0);

  // Wait and then simulate a user-initiated brightness change.
  int seconds_to_wait = 5;
  AdvanceClock(base::Seconds(seconds_to_wait));
  keyboard_brightness_control_delegate()->HandleSetKeyboardBrightness(
      50, true, KeyboardBrightnessChangeSource::kSettingsApp);

  // Verify that the user-initiated brightness change is recorded.
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Keyboard.TimeUntilFirstBrightnessChange.AfterLogin."
      "SetBrightness.BatteryPower",
      base::Seconds(seconds_to_wait), 1);
}

}  // namespace ash
