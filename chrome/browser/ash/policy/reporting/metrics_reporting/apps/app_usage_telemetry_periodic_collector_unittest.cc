// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_telemetry_periodic_collector.h"

#include <memory>
#include <optional>

#include "base/test/task_environment.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting {
namespace {

class AppUsageTelemetryPeriodicCollectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ::ash::SessionManagerClient::InitializeFakeInMemory();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  ::ash::SessionTerminationManager session_termination_manager_;
  test::FakeReportingSettings reporting_settings_;
  test::FakeSampler sampler_;
  test::FakeMetricReportQueue metric_report_queue_;
};

TEST_F(AppUsageTelemetryPeriodicCollectorTest,
       CollectMetricDataWhenRateNotSet) {
  // Set up test sampler to report telemetry data.
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  // Fast forward timer to trigger telemetry collection and verify data being
  // reported.
  const AppUsageTelemetryPeriodicCollector collector(
      &sampler_, &metric_report_queue_, &reporting_settings_);
  task_environment_.FastForwardBy(
      metrics::kDefaultAppUsageTelemetryCollectionRate);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(1));
  ASSERT_FALSE(metric_report_queue_.IsEmpty());
  const auto& enqueued_metric_data =
      metric_report_queue_.GetMetricDataReported();
  EXPECT_TRUE(enqueued_metric_data.has_timestamp_ms());
  EXPECT_TRUE(enqueued_metric_data.has_telemetry_data());
}

TEST_F(AppUsageTelemetryPeriodicCollectorTest, CollectMetricDataWhenRateSet) {
  // Set collection rate via policy setting.
  static constexpr base::TimeDelta kCollectionRate = base::Minutes(5);
  reporting_settings_.SetInteger(
      ::ash::reporting::kReportAppUsageCollectionRateMs,
      kCollectionRate.InMilliseconds());

  // Set up sampler to report telemetry data.
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  // Fast forward timer to trigger telemetry collection and verify data being
  // reported.
  const AppUsageTelemetryPeriodicCollector collector(
      &sampler_, &metric_report_queue_, &reporting_settings_);
  task_environment_.FastForwardBy(kCollectionRate);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(1));
  const auto& enqueued_metric_data =
      metric_report_queue_.GetMetricDataReported();
  EXPECT_TRUE(enqueued_metric_data.has_timestamp_ms());
  EXPECT_TRUE(enqueued_metric_data.has_telemetry_data());
}

TEST_F(AppUsageTelemetryPeriodicCollectorTest,
       CollectMetricDataWhenRateUpdated) {
  // Set initial collection rate via policy setting.
  static constexpr base::TimeDelta kCollectionRate = base::Minutes(5);
  reporting_settings_.SetInteger(
      ::ash::reporting::kReportAppUsageCollectionRateMs,
      kCollectionRate.InMilliseconds());

  // Set up sampler to report telemetry data.
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));
  const AppUsageTelemetryPeriodicCollector collector(
      &sampler_, &metric_report_queue_, &reporting_settings_);

  // Update collection rate setting before triggering collection. This is
  // so the rate controller can pick up updated setting value on collection and
  // configure subsequent ones accordingly.
  static constexpr base::TimeDelta kNewCollectionRate =
      kCollectionRate + kCollectionRate;
  reporting_settings_.SetInteger(
      ::ash::reporting::kReportAppUsageCollectionRateMs,
      kNewCollectionRate.InMilliseconds());

  // Fast forward timer to trigger telemetry collection and verify data being
  // reported.
  {
    task_environment_.FastForwardBy(kCollectionRate);
    ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(1));
    const auto& enqueued_metric_data =
        metric_report_queue_.GetMetricDataReported();
    ASSERT_TRUE(enqueued_metric_data.has_timestamp_ms());
    ASSERT_TRUE(enqueued_metric_data.has_telemetry_data());
  }

  // Advance timer by old collection rate and verify no data is being reported.
  task_environment_.FastForwardBy(kCollectionRate);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(1));
  ASSERT_TRUE(metric_report_queue_.IsEmpty());

  // Advance timer by the new collection rate and verify data being reported.
  {
    task_environment_.FastForwardBy(kNewCollectionRate);
    ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(2));
    const auto& enqueued_metric_data =
        metric_report_queue_.GetMetricDataReported();
    EXPECT_TRUE(enqueued_metric_data.has_timestamp_ms());
    EXPECT_TRUE(enqueued_metric_data.has_telemetry_data());
  }
}

TEST_F(AppUsageTelemetryPeriodicCollectorTest, CollectEmptyMetricData) {
  // Set up sampler to report empty metric data.
  sampler_.SetMetricData(std::nullopt);

  // Fast forward timer to trigger telemetry collection and verify no data is
  // being reported.
  const AppUsageTelemetryPeriodicCollector collector(
      &sampler_, &metric_report_queue_, &reporting_settings_);
  task_environment_.FastForwardBy(
      metrics::kDefaultAppUsageTelemetryCollectionRate);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(1));
  ASSERT_TRUE(metric_report_queue_.IsEmpty());
}

TEST_F(AppUsageTelemetryPeriodicCollectorTest, OnCollectorDestruction) {
  // Set up test sampler to report data.
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  // Set up periodic collector and destroy collector before triggering
  // collection.
  auto collector = std::make_unique<AppUsageTelemetryPeriodicCollector>(
      &sampler_, &metric_report_queue_, &reporting_settings_);
  collector.reset();

  // Fast forward timer and verify no data is being reported.
  task_environment_.FastForwardBy(
      metrics::kDefaultAppUsageTelemetryCollectionRate);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));
  ASSERT_TRUE(metric_report_queue_.IsEmpty());
}

TEST_F(AppUsageTelemetryPeriodicCollectorTest, OnSessionTermination) {
  // Set up test sampler to report data.
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  // Set up periodic collector and terminate session.
  const AppUsageTelemetryPeriodicCollector collector(
      &sampler_, &metric_report_queue_, &reporting_settings_);
  session_termination_manager_.StopSession(
      ::login_manager::SessionStopReason::USER_REQUESTS_SIGNOUT);

  // Verify data being reported.
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(1));
  const auto& enqueued_metric_data =
      metric_report_queue_.GetMetricDataReported();
  ASSERT_TRUE(enqueued_metric_data.has_timestamp_ms());
  ASSERT_TRUE(enqueued_metric_data.has_telemetry_data());
}

}  // namespace
}  // namespace reporting
