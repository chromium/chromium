// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/shell.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/user_manager/known_user.h"

namespace ash {

namespace {

constexpr char kUserEmail[] = "user@example.com";
constexpr char kUserEmailSecondary[] = "user2@example.com";

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
  void HandleSetKeyboardBrightness(double percent, bool gradual) override {
    keyboard_brightness_ = percent;
  }
  void HandleSetKeyboardAmbientLightSensorEnabled(bool enabled) override {}

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

 private:
  double keyboard_brightness_ = 0;
};

class KeyboardBrightnessControllerTest : public AshTestBase {
 public:
  KeyboardBrightnessControllerTest() = default;

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

  void SetKeyboardAmbientLightSensorEnabled(
      bool enabled,
      power_manager::AmbientLightSensorChange_Cause cause) {
    keyboard_brightness_control_delegate()
        ->HandleSetKeyboardAmbientLightSensorEnabled(enabled);
    power_manager::AmbientLightSensorChange sensor_change;
    sensor_change.set_sensor_enabled(enabled);
    sensor_change.set_cause(cause);
    power_manager_client()->SendKeyboardAmbientLightSensorEnabledChanged(
        sensor_change);
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

TEST_F(KeyboardBrightnessControllerTest, SetKeyboardAmbientLightSensorEnabled) {
  // Ambient light sensor is enabled by default.
  EXPECT_TRUE(power_manager_client()->keyboard_ambient_light_sensor_enabled());
  // Disable the ambient light sensor.
  keyboard_brightness_control_delegate()
      ->HandleSetKeyboardAmbientLightSensorEnabled(false);

  // Verify that the ambient light sensor is now disabled.
  EXPECT_FALSE(power_manager_client()->keyboard_ambient_light_sensor_enabled());

  keyboard_brightness_control_delegate()
      ->HandleGetKeyboardAmbientLightSensorEnabled(base::BindOnce(
          [](std::optional<bool> is_ambient_light_sensor_enabled) {
            EXPECT_FALSE(is_ambient_light_sensor_enabled.value());
          }));

  // Re-enabled the ambient light sensor
  keyboard_brightness_control_delegate()
      ->HandleSetKeyboardAmbientLightSensorEnabled(true);

  // Verify that the ambient light sensor is enabled.
  EXPECT_TRUE(power_manager_client()->keyboard_ambient_light_sensor_enabled());

  keyboard_brightness_control_delegate()
      ->HandleGetKeyboardAmbientLightSensorEnabled(base::BindOnce(
          [](std::optional<bool> is_ambient_light_sensor_enabled) {
            EXPECT_TRUE(is_ambient_light_sensor_enabled.value());
          }));
}

TEST_F(KeyboardBrightnessControllerTest, SavePrefToKnownUser) {
  // Set initial ALS status.
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(true);

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

TEST_F(KeyboardBrightnessControllerTest,
       RestoreKeyboardAmbientLightSensorEnabled_FlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Set initial ALS status.
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(true);

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, focus the first user.
  AccountId first_account = AccountId::FromUserEmail(kUserEmail);
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // Then, focus the second user.
  AccountId second_account = AccountId::FromUserEmail(kUserEmailSecondary);
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // Switch to first user and disable ALS.
  LoginScreenFocusAccount(first_account);
  keyboard_brightness_control_delegate()
      ->HandleSetKeyboardAmbientLightSensorEnabled(false);

  // ALS should be disabled for the first user.
  run_loop_.RunUntilIdle();
  ExpectKeyboardAmbientLightSensorEnabled(false);

  // ALS should remain enabled for the second user, despite being disabled for
  // the first user.
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // ALS should be disabled for first user after switching back from second
  // user.
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(false);

  // Simulate a reboot, which resets the value of the ambient light sensor.
  ClearLogin();
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(true);

  // After reboot, ALS should be still be disabled for the first user
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(false);

  // After reboot, ALS should be still be enabled for second user.
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // Switch back to the first user, ALS should remain disabled.
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(false);
}

TEST_F(KeyboardBrightnessControllerTest,
       RestoreKeyboardAmbientLightSensorEnabled_FlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kEnableKeyboardBacklightControlInSettings);

  // Set initial ALS status.
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(true);

  // Clear user sessions and reset to the primary login screen.
  ClearLogin();

  // On the login screen, focus the first user.
  AccountId first_account = AccountId::FromUserEmail(kUserEmail);
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // Then, focus the second user.
  AccountId second_account = AccountId::FromUserEmail(kUserEmailSecondary);
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // Switch to first user and disable ALS.
  LoginScreenFocusAccount(first_account);
  keyboard_brightness_control_delegate()
      ->HandleSetKeyboardAmbientLightSensorEnabled(false);

  // ALS should be disabled for the first user.
  run_loop_.RunUntilIdle();
  ExpectKeyboardAmbientLightSensorEnabled(false);

  // ALS should be disabled for second user because the ALS value is not being
  // restored from prefs.
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(false);

  // ALS should be disabled for first user after switching back from second
  // user.
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(false);

  // Simulate a reboot, which resets the value of the ambient light sensor.
  ClearLogin();
  power_manager_client()->SetKeyboardAmbientLightSensorEnabled(true);

  // After reboot, ALS should be enabled for the first user by default.
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // After reboot, ALS should be enabled for the second user by default.
  LoginScreenFocusAccount(second_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);

  // Switch back to the first user, ALS should remain enabled.
  LoginScreenFocusAccount(first_account);
  ExpectKeyboardAmbientLightSensorEnabled(true);
}

}  // namespace ash
