// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/device_activity/device_activity_sampler.h"

#include <optional>

#include "base/test/task_environment.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/scoped_set_idle_state.h"

using ::testing::Eq;
using ::testing::ValuesIn;

namespace reporting {
namespace {

class DeviceActivitySamplerTest
    : public ::testing::TestWithParam<::ui::IdleState> {
 protected:
  UserStatusTelemetry::DeviceActivityState GetExpectedDeviceActivityState() {
    ::ui::IdleState idle_state = GetParam();
    switch (idle_state) {
      case ::ui::IdleState::IDLE_STATE_IDLE:
        return UserStatusTelemetry::IDLE;
      case ::ui::IdleState::IDLE_STATE_LOCKED:
        return UserStatusTelemetry::LOCKED;
      case ::ui::IdleState::IDLE_STATE_ACTIVE:
        return UserStatusTelemetry::ACTIVE;
      case ::ui::IdleState::IDLE_STATE_UNKNOWN:
        return UserStatusTelemetry::DEVICE_ACTIVITY_STATE_UNKNOWN;
    }
  }

  base::test::TaskEnvironment task_environment_;
  DeviceActivitySampler sampler_;
};

TEST_P(DeviceActivitySamplerTest, CollectDeviceActivityState) {
  ::ui::ScopedSetIdleState scoped_set_idle_state(GetParam());
  test::TestEvent<std::optional<MetricData>> test_event;
  sampler_.MaybeCollect(test_event.cb());
  std::optional<MetricData> result = test_event.result();
  ASSERT_TRUE(result.has_value());
  const MetricData& metric_data = result.value();
  ASSERT_TRUE(metric_data.has_telemetry_data()) << "Missing telemetry data";
  ASSERT_TRUE(metric_data.telemetry_data().has_user_status_telemetry())
      << "Missing user status telemetry data";
  EXPECT_THAT(metric_data.telemetry_data()
                  .user_status_telemetry()
                  .device_activity_state(),
              Eq(GetExpectedDeviceActivityState()));
}

INSTANTIATE_TEST_SUITE_P(DeviceActivitySamplerTests,
                         DeviceActivitySamplerTest,
                         ValuesIn({::ui::IdleState::IDLE_STATE_ACTIVE,
                                   ::ui::IdleState::IDLE_STATE_IDLE,
                                   ::ui::IdleState::IDLE_STATE_LOCKED,
                                   ::ui::IdleState::IDLE_STATE_UNKNOWN}));

}  // namespace
}  // namespace reporting
