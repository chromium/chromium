// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/performance_monitor/process_monitor.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using UkmEntry = ukm::builders::PowerUsageScenariosIntervalData;

class TestPowerMonitor : public performance_monitor::ProcessMonitor {
 public:
  TestPowerMonitor() = default;
  TestPowerMonitor(const TestPowerMonitor& rhs) = delete;
  TestPowerMonitor& operator=(const TestPowerMonitor& rhs) = delete;
  ~TestPowerMonitor() override = default;

  // Call OnAggregatedMetricsSampled for all the observers with |metrics| as an
  // argument.
  void NotifyObserversForOnAggregatedMetricsSampled(const Metrics& metrics) {
    for (auto& obs : GetObserversForTesting())
      obs.OnAggregatedMetricsSampled(metrics);
  }
};

class TestUsageScenarioDataStoreImpl : public UsageScenarioDataStoreImpl {
 public:
  TestUsageScenarioDataStoreImpl() = default;
  TestUsageScenarioDataStoreImpl(const TestUsageScenarioDataStoreImpl& rhs) =
      delete;
  TestUsageScenarioDataStoreImpl& operator=(
      const TestUsageScenarioDataStoreImpl& rhs) = delete;
  ~TestUsageScenarioDataStoreImpl() override = default;

  IntervalData ResetIntervalData() override { return fake_data_; }

  void SetIntervalDataToReturn(IntervalData data) { fake_data_ = data; }

 private:
  IntervalData fake_data_;
};

// This doesn't use the typical {class being tested}Test name pattern because
// there's already a PowerMetricsReporterTest class in the chromeos namespace
// and this conflicts with it.
class PowerMetricsReporterUnitTest : public testing::Test {
 public:
  PowerMetricsReporterUnitTest()
      : power_metrics_reporter_(data_store_.GetWeakPtr()) {}

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestPowerMonitor power_monitor_;
  TestUsageScenarioDataStoreImpl data_store_;
  PowerMetricsReporter power_metrics_reporter_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

}  // namespace

TEST_F(PowerMetricsReporterUnitTest, UKMs) {
  UsageScenarioDataStore::IntervalData fake_interval_data;

  int fake_value = 42;
  fake_interval_data.uptime_at_interval_end =
      base::TimeDelta::FromHours(++fake_value);
  fake_interval_data.max_tab_count = ++fake_value;
  fake_interval_data.max_visible_window_count = ++fake_value;
  fake_interval_data.top_level_navigation_count = ++fake_value;
  fake_interval_data.tabs_closed_during_interval = ++fake_value;
  fake_interval_data.user_interaction_count = ++fake_value;
  fake_interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta::FromSeconds(++fake_value);
  fake_interval_data.time_with_open_webrtc_connection =
      base::TimeDelta::FromSeconds(++fake_value);
  fake_interval_data.source_id_for_longest_visible_origin = ++fake_value;
  fake_interval_data.source_id_for_longest_visible_origin_duration =
      base::TimeDelta::FromSeconds(++fake_value);

  const int kExpectedIntervalLengthSeconds = ++fake_value;
  task_environment_.FastForwardBy(
      base::TimeDelta::FromSeconds(kExpectedIntervalLengthSeconds));

  data_store_.SetIntervalDataToReturn(fake_interval_data);

  performance_monitor::ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = ++fake_value * 0.01;
#if defined(OS_MAC)
  fake_metrics.idle_wakeups = ++fake_value;
  fake_metrics.package_idle_wakeups = ++fake_value;
  fake_metrics.energy_impact = ++fake_value;
#endif

  power_monitor_.NotifyObserversForOnAggregatedMetricsSampled(fake_metrics);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kUptimeSecondsName,
      ukm::GetExponentialBucketMinForUserTiming(
          fake_interval_data.uptime_at_interval_end.InSeconds()));
  // TODO(sebmarchand): Update once the proper value is recorded.
  test_ukm_recorder_.ExpectEntryMetric(entries[0],
                                       UkmEntry::kBatteryDischargeRateName, 0);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kCPUTimeMsName,
      kExpectedIntervalLengthSeconds * 1000 * fake_metrics.cpu_usage);
#if defined(OS_MAC)
  test_ukm_recorder_.ExpectEntryMetric(entries[0], UkmEntry::kIdleWakeUpsName,
                                       fake_metrics.idle_wakeups);
  test_ukm_recorder_.ExpectEntryMetric(entries[0], UkmEntry::kPackageExitsName,
                                       fake_metrics.package_idle_wakeups);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kEnergyImpactScoreName, fake_metrics.energy_impact);
#endif
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kMaxTabCountName,
      ukm::GetExponentialBucketMinForCounts1000(
          fake_interval_data.max_tab_count));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kMaxVisibleWindowCountName,
      ukm::GetExponentialBucketMin(fake_interval_data.max_visible_window_count,
                                   1.05));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kTabClosedName,
      ukm::GetExponentialBucketMinForCounts1000(
          fake_interval_data.tabs_closed_during_interval));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kTopLevelNavigationEventsName,
      ukm::GetExponentialBucketMinForCounts1000(
          fake_interval_data.top_level_navigation_count));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kUserInteractionCountName,
      ukm::GetExponentialBucketMinForCounts1000(
          fake_interval_data.user_interaction_count));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kFullscreenVideoSingleMonitorSecondsName,
      ukm::GetExponentialBucketMinForUserTiming(
          fake_interval_data.time_playing_video_full_screen_single_monitor
              .InSeconds()));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kTimeWithOpenWebRTCConnectionSecondsName,
      ukm::GetExponentialBucketMinForUserTiming(
          fake_interval_data.time_with_open_webrtc_connection.InSeconds()));
  test_ukm_recorder_.ExpectEntryMetric(entries[0],
                                       UkmEntry::kIntervalDurationSecondsName,
                                       kExpectedIntervalLengthSeconds);
}
