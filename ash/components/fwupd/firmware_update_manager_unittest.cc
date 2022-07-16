// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/fwupd/firmware_update_manager.h"

#include <memory>

#include "chromeos/dbus/fwupd/fake_fwupd_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class FirmwareUpdateManagerTest : public testing::Test {
 public:
  FirmwareUpdateManagerTest() {}
  FirmwareUpdateManagerTest(const FirmwareUpdateManagerTest&) = delete;
  FirmwareUpdateManagerTest& operator=(const FirmwareUpdateManagerTest&) =
      delete;
  ~FirmwareUpdateManagerTest() override = default;

  int GetOnDevicesResponseCallbackCallCountForTesting() {
    return firmware_update_manager_.on_device_list_response_count_for_testing_;
  }

  chromeos::FakeFwupdClient dbus_client_;
  FirmwareUpdateManager firmware_update_manager_;
};

// TODO(swifton): Rewrite this test with an observer.
TEST_F(FirmwareUpdateManagerTest, RequestDeviceList) {
  // FirmwareUpdateManager requests devices when it is created.
  EXPECT_EQ(1, GetOnDevicesResponseCallbackCallCountForTesting());
  firmware_update_manager_.RequestDevices();
  EXPECT_EQ(2, GetOnDevicesResponseCallbackCallCountForTesting());
}

}  // namespace ash
