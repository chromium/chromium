// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_dispatcher.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class InputDeviceSettingsDispatcherTest : public AshTestBase {
 public:
  InputDeviceSettingsDispatcherTest() = default;
  InputDeviceSettingsDispatcherTest(const InputDeviceSettingsDispatcherTest&) =
      delete;
  InputDeviceSettingsDispatcherTest& operator=(
      const InputDeviceSettingsDispatcherTest&) = delete;
  ~InputDeviceSettingsDispatcherTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    dispatcher_ = std::make_unique<InputDeviceSettingsDispatcher>();
  }

  void TearDown() override {
    dispatcher_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDeviceSettingsDispatcher> dispatcher_;
};

TEST_F(InputDeviceSettingsDispatcherTest, InitializationTest) {
  EXPECT_NE(dispatcher_.get(), nullptr);
}

}  // namespace ash
