// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_pref_manager.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class InputDevicePrefManagerTest : public AshTestBase {
 public:
  InputDevicePrefManagerTest() = default;
  InputDevicePrefManagerTest(const InputDevicePrefManagerTest&) = delete;
  InputDevicePrefManagerTest& operator=(const InputDevicePrefManagerTest&) =
      delete;
  ~InputDevicePrefManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<InputDevicePrefManager>();
  }

  void TearDown() override {
    controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDevicePrefManager> controller_;
};

TEST_F(InputDevicePrefManagerTest, InitializationTest) {
  EXPECT_NE(controller_.get(), nullptr);
}

}  // namespace ash
