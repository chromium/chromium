// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_client.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace device_activity {

class DeviceActivityClientTest : public testing::Test {
 public:
  DeviceActivityClientTest() = default;
  DeviceActivityClientTest(const DeviceActivityClientTest&) = delete;
  DeviceActivityClientTest& operator=(const DeviceActivityClientTest&) = delete;
  ~DeviceActivityClientTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    device_activity_client_ =
        std::make_unique<DeviceActivityClient>(Trigger::kNetwork);
  }

  void TearDown() override { device_activity_client_.reset(); }

  std::unique_ptr<DeviceActivityClient> device_activity_client_;
};

}  // namespace device_activity
}  // namespace ash
