// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_controller.h"

#include "ash/components/device_activity/fresnel_pref_names.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
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

  TestingPrefServiceSimple* local_state() { return &pref_service_; }

 protected:
  // testing::Test:
  void SetUp() override {
    DeviceActivityController::RegisterPrefs(local_state()->registry());
    device_activity_controller_ = std::make_unique<DeviceActivityController>();
  }

  void TearDown() override { device_activity_controller_.reset(); }

  std::unique_ptr<DeviceActivityController> device_activity_controller_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple pref_service_;
};

TEST_F(DeviceActivityControllerTest,
       CheckDeviceActivityControllerSingletonInitialized) {
  EXPECT_NE(DeviceActivityController::Get(), nullptr);
}

TEST_F(DeviceActivityControllerTest,
       CheckLocalStatePingTimestampsInitializedToUnixEpoch) {
  base::Time daily_ts =
      local_state()->GetTime(prefs::kDeviceActiveLastKnownDailyPingTimestamp);
  EXPECT_EQ(daily_ts, base::Time::UnixEpoch());

  base::Time monthly_ts =
      local_state()->GetTime(prefs::kDeviceActiveLastKnownMonthlyPingTimestamp);
  EXPECT_EQ(monthly_ts, base::Time::UnixEpoch());

  base::Time alltime_ts =
      local_state()->GetTime(prefs::kDeviceActiveLastKnownAllTimePingTimestamp);
  EXPECT_EQ(alltime_ts, base::Time::UnixEpoch());
}

}  // namespace device_activity
}  // namespace ash
