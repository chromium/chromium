// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_usage_telemetry_periodic_collector_ash.h"

#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting {
namespace {

constexpr base::TimeDelta kCollectionInterval = base::Minutes(25);

class WebsiteUsageTelemetryPeriodicCollectorAshTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ::ash::SessionManagerClient::InitializeFakeInMemory();
  }

  void AssertDataIsReported() {
    ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(1));
    const auto enqueued_metric_data =
        metric_report_queue_.GetMetricDataReported();
    EXPECT_TRUE(enqueued_metric_data.has_timestamp_ms());
    EXPECT_TRUE(enqueued_metric_data.has_telemetry_data());
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ::ash::SessionTerminationManager session_termination_manager_;
  test::FakeReportingSettings reporting_settings_;
  test::FakeSampler sampler_;
  test::FakeMetricReportQueue metric_report_queue_;
};

TEST_F(WebsiteUsageTelemetryPeriodicCollectorAshTest,
       CollectMetricDataWithDefaultRate) {
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  const WebsiteUsageTelemetryPeriodicCollectorAsh
      website_usage_telemetry_periodic_collector(
          &sampler_, &metric_report_queue_, &reporting_settings_);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));

  // Advance timer and verify data is collected.
  task_environment_.FastForwardBy(
      metrics::kDefaultWebsiteTelemetryCollectionRate);
  AssertDataIsReported();
}

TEST_F(WebsiteUsageTelemetryPeriodicCollectorAshTest,
       CollectMetricDataWithRateSetting) {
  reporting_settings_.SetInteger(kReportWebsiteTelemetryCollectionRateMs,
                                 kCollectionInterval.InMilliseconds());
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  const WebsiteUsageTelemetryPeriodicCollectorAsh
      website_usage_telemetry_periodic_collector(
          &sampler_, &metric_report_queue_, &reporting_settings_);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));

  // Advance timer and verify data is collected.
  task_environment_.FastForwardBy(kCollectionInterval);
  AssertDataIsReported();
}

TEST_F(WebsiteUsageTelemetryPeriodicCollectorAshTest, NoMetricData) {
  reporting_settings_.SetInteger(kReportWebsiteTelemetryCollectionRateMs,
                                 kCollectionInterval.InMilliseconds());
  const WebsiteUsageTelemetryPeriodicCollectorAsh
      website_usage_telemetry_periodic_collector(
          &sampler_, &metric_report_queue_, &reporting_settings_);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));

  // Advance timer and verify no data is reported.
  task_environment_.FastForwardBy(kCollectionInterval);
  ASSERT_TRUE(metric_report_queue_.IsEmpty());
}

TEST_F(WebsiteUsageTelemetryPeriodicCollectorAshTest, OnCollectorDestruction) {
  reporting_settings_.SetInteger(kReportWebsiteTelemetryCollectionRateMs,
                                 kCollectionInterval.InMilliseconds());
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  // Set up periodic collector and destroy collector before triggering
  // collection.
  auto website_usage_telemetry_periodic_collector =
      std::make_unique<WebsiteUsageTelemetryPeriodicCollectorAsh>(
          &sampler_, &metric_report_queue_, &reporting_settings_);
  website_usage_telemetry_periodic_collector.reset();

  // Advance timer and verify no data is collected.
  task_environment_.FastForwardBy(kCollectionInterval);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));
  ASSERT_TRUE(metric_report_queue_.IsEmpty());
}

TEST_F(WebsiteUsageTelemetryPeriodicCollectorAshTest, OnSessionTermination) {
  reporting_settings_.SetInteger(kReportWebsiteTelemetryCollectionRateMs,
                                 kCollectionInterval.InMilliseconds());
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  // Set up periodic collector and terminate session.
  const WebsiteUsageTelemetryPeriodicCollectorAsh
      website_usage_telemetry_periodic_collector(
          &sampler_, &metric_report_queue_, &reporting_settings_);
  session_termination_manager_.StopSession(
      ::login_manager::SessionStopReason::USER_REQUESTS_SIGNOUT);

  AssertDataIsReported();
}

}  // namespace
}  // namespace reporting
