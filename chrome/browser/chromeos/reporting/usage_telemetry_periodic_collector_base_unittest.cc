// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/usage_telemetry_periodic_collector_base.h"

#include <optional>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting {
namespace {

constexpr char kRateSettingPath[] = "rate_setting_path";
constexpr base::TimeDelta kDefaultInterval = base::Minutes(2);
constexpr base::TimeDelta kInterval = base::Minutes(5);

class UsageTelemetryPeriodicCollectorBaseTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::FakeReportingSettings reporting_settings_;
  test::FakeSampler sampler_;
  test::FakeMetricReportQueue metric_report_queue_;
};

TEST_F(UsageTelemetryPeriodicCollectorBaseTest,
       CollectUsageTelemetryWithDefaultRate) {
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  UsageTelemetryPeriodicCollectorBase usage_telemetry_periodic_collector(
      &sampler_, &metric_report_queue_, &reporting_settings_, kRateSettingPath,
      kDefaultInterval);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));

  // Advance timer and verify data is collected.
  task_environment_.FastForwardBy(kDefaultInterval);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(1));
  const auto metric_data_reported =
      metric_report_queue_.GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
}

TEST_F(UsageTelemetryPeriodicCollectorBaseTest,
       CollectUsageTelemetryWithRateSetting) {
  reporting_settings_.SetInteger(kRateSettingPath, kInterval.InMilliseconds());
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  UsageTelemetryPeriodicCollectorBase usage_telemetry_periodic_collector(
      &sampler_, &metric_report_queue_, &reporting_settings_, kRateSettingPath,
      kDefaultInterval);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));

  // Advance timer and verify data is collected.
  task_environment_.FastForwardBy(kInterval);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(1));
  const auto metric_data_reported =
      metric_report_queue_.GetMetricDataReported();
  EXPECT_TRUE(metric_data_reported.has_timestamp_ms());
  EXPECT_TRUE(metric_data_reported.has_telemetry_data());
}

TEST_F(UsageTelemetryPeriodicCollectorBaseTest, NoMetricData) {
  reporting_settings_.SetInteger(kRateSettingPath, kInterval.InMilliseconds());

  UsageTelemetryPeriodicCollectorBase usage_telemetry_periodic_collector(
      &sampler_, &metric_report_queue_, &reporting_settings_, kRateSettingPath,
      kDefaultInterval);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));

  // Advance timer and verify no data is reported.
  task_environment_.FastForwardBy(kInterval);
  ASSERT_TRUE(metric_report_queue_.IsEmpty());
}

}  // namespace
}  // namespace reporting
