// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_controller.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace device_activity {

class DeviceActivityControllerTest : public testing::Test {
 public:
  DeviceActivityControllerTest() = default;
  DeviceActivityControllerTest(const DeviceActivityControllerTest&) = delete;
  DeviceActivityControllerTest& operator=(const DeviceActivityControllerTest&) =
      delete;
  ~DeviceActivityControllerTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    device_activity_controller_ = std::make_unique<DeviceActivityController>();
  }

  void TearDown() override { device_activity_controller_.reset(); }

  std::unique_ptr<DeviceActivityController> device_activity_controller_;
};

TEST_F(DeviceActivityControllerTest,
       CheckDeviceActivityControllerSingletonInitialized) {
  EXPECT_NE(DeviceActivityController::Get(), nullptr);
}

}  // namespace device_activity
}  // namespace ash
