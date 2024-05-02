// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"

#include "ash/shell.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"

namespace ash {

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

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<FakeKeyboardBrightnessControlDelegate> delegate_;
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

}  // namespace ash
