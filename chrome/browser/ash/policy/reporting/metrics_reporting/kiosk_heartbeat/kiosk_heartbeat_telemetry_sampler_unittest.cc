// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/kiosk_heartbeat/kiosk_heartbeat_telemetry_sampler.h"

#include <optional>

#include "base/test/task_environment.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {
class KioskHeartbeatTelemetrySamplerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  KioskHeartbeatTelemetrySampler sampler_;
};

TEST_F(KioskHeartbeatTelemetrySamplerTest, MaybeCollect) {
  test::TestEvent<std::optional<MetricData>> test_event;
  sampler_.MaybeCollect(test_event.cb());
  std::optional<MetricData> result = test_event.result();

  ASSERT_TRUE(result.has_value());
  const MetricData& metric_data = result.value();
  ASSERT_TRUE(metric_data.has_telemetry_data()) << "Missing telemetry data";
  ASSERT_TRUE(metric_data.telemetry_data().has_heartbeat_telemetry())
      << "Missing heartbeat telemetry data";
}
}  // namespace
}  // namespace reporting
