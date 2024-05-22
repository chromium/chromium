// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/metrics/structured_metrics_service_wrapper.h"

#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/test/test_structured_metrics_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;
}  // namespace

class StructuredMetricsServiceWrapperTest : public testing::Test {
 protected:
  StructuredMetricsServiceWrapperTest() = default;
  StructuredMetricsServiceWrapperTest(
      const StructuredMetricsServiceWrapperTest&) = delete;
  StructuredMetricsServiceWrapperTest& operator=(
      const StructuredMetricsServiceWrapperTest&) = delete;
  ~StructuredMetricsServiceWrapperTest() override = default;

  void SetUp() override {
    sm_service_ = std::make_unique<StructuredMetricsServiceWrapper>();

    metrics_recorder_ =
        std::make_unique<metrics::structured::TestStructuredMetricsRecorder>();
    metrics_recorder_->Initialize();
  }

  void TearDown() override {
    sm_service_.reset();
    metrics_recorder_.reset();
  }

 protected:
  std::unique_ptr<StructuredMetricsServiceWrapper> sm_service_;
  std::unique_ptr<metrics::structured::TestStructuredMetricsRecorder>
      metrics_recorder_;
};

TEST_F(StructuredMetricsServiceWrapperTest,
       TestRecordCrosEvent_HasEventTimestamp) {
  cros_events::CameraApp_StartSession expected_event;
  expected_event.SetLaunchType(
      static_cast<cros_events::CameraAppLaunchType>(1));
  expected_event.SetLanguage(5);

  cros_events::CameraApp_StartSession input_event;
  input_event.SetLaunchType(static_cast<cros_events::CameraAppLaunchType>(1));
  input_event.SetLanguage(5);
  input_event.SetRecordedTimeSinceBoot(base::Seconds(1000));

  std::vector<metrics::structured::Event> events;
  events.emplace_back(std::move(input_event));
  sm_service_->Record(std::move(events));

  const std::vector<metrics::structured::Event>& recorded_events =
      metrics_recorder_->GetEvents();
  ASSERT_EQ(recorded_events.size(), 1U);

  auto& received_event = recorded_events[0];
  EXPECT_EQ(received_event.event_name(), expected_event.event_name());
  EXPECT_EQ(received_event.metric_values(), expected_event.metric_values());
  EXPECT_TRUE(received_event.has_system_uptime());
}

TEST_F(StructuredMetricsServiceWrapperTest, TestComputeEventUptime_ValidTimes) {
  base::TimeDelta system_uptime = base::Seconds(4000);
  base::TimeDelta system_timestamp = base::Seconds(10000);
  base::TimeDelta event_timestamp = base::Seconds(7000);
  base::TimeDelta expected_event_uptime = base::Seconds(1000);
  EXPECT_EQ(expected_event_uptime,
            sm_service_->ComputeEventUptime(system_uptime, system_timestamp,
                                            event_timestamp));
}

TEST_F(StructuredMetricsServiceWrapperTest,
       TestComputeEventUptime_EventTimestampLaterThanSystemTimestamp) {
  base::TimeDelta system_uptime = base::Seconds(4000);
  base::TimeDelta system_timestamp = base::Seconds(10000);
  base::TimeDelta event_timestamp = base::Seconds(11000);
  base::TimeDelta expected_event_uptime = base::Seconds(4000);
  EXPECT_EQ(expected_event_uptime,
            sm_service_->ComputeEventUptime(system_uptime, system_timestamp,
                                            event_timestamp));
}
}  // namespace ash
