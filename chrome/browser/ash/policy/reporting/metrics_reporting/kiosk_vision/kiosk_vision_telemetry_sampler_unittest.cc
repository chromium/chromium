// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/kiosk_vision/kiosk_vision_telemetry_sampler.h"

#include <optional>

#include "base/test/task_environment.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {

class KioskVisionTelemetrySamplerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  KioskVisionTelemetrySampler sampler_;
};

TEST_F(KioskVisionTelemetrySamplerTest, MaybeCollect) {
  test::TestEvent<std::optional<MetricData>> test_event;
  sampler_.MaybeCollect(test_event.cb());
  std::optional<MetricData> result = test_event.result();

  ASSERT_TRUE(result.has_value());
  const MetricData& metric_data = result.value();
  ASSERT_TRUE(metric_data.has_telemetry_data()) << "Missing telemetry data";
  EXPECT_TRUE(metric_data.telemetry_data().has_kiosk_vision_telemetry())
      << "Missing kiosk vision telemetry";
  EXPECT_TRUE(metric_data.telemetry_data().has_kiosk_vision_status())
      << "Missing kiosk vision status";
}

}  // namespace reporting
