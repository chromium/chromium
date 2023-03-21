// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class InputDeviceSettingsMetricsManagerTest : public AshTestBase {
 public:
  InputDeviceSettingsMetricsManagerTest() = default;
  InputDeviceSettingsMetricsManagerTest(
      const InputDeviceSettingsMetricsManagerTest&) = delete;
  InputDeviceSettingsMetricsManagerTest& operator=(
      const InputDeviceSettingsMetricsManagerTest&) = delete;
  ~InputDeviceSettingsMetricsManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    manager_ = std::make_unique<InputDeviceSettingsMetricsManager>();
  }

  void TearDown() override {
    manager_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDeviceSettingsMetricsManager> manager_;
};

TEST_F(InputDeviceSettingsMetricsManagerTest, InitializationTest) {
  EXPECT_NE(manager_.get(), nullptr);
}

}  // namespace ash