// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_policy_handler.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class InputDeviceSettingsPolicyHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    handler_ = std::make_unique<InputDeviceSettingsPolicyHandler>();
  }

 protected:
  std::unique_ptr<InputDeviceSettingsPolicyHandler> handler_;
};

TEST_F(InputDeviceSettingsPolicyHandlerTest, InitializationTest) {
  EXPECT_NE(nullptr, handler_.get());
}

}  // namespace ash
