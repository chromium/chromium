// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_controller.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class InputDeviceSettingsControllerTest : public AshTestBase {
 public:
  InputDeviceSettingsControllerTest() = default;
  InputDeviceSettingsControllerTest(const InputDeviceSettingsControllerTest&) =
      delete;
  InputDeviceSettingsControllerTest& operator=(
      const InputDeviceSettingsControllerTest&) = delete;
  ~InputDeviceSettingsControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<InputDeviceSettingsController>();
  }

  void TearDown() override {
    controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDeviceSettingsController> controller_;
};

TEST_F(InputDeviceSettingsControllerTest, PlaceholderTest) {
  EXPECT_NE(controller_.get(), nullptr);
}

}  // namespace ash
