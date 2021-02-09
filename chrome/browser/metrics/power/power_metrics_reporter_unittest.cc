// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

#include <memory>

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

class PowerMetricsReporterAccess : public PowerMetricsReporter {
 public:
  using PowerMetricsReporter::kBatteryStateChangedValue;
  using PowerMetricsReporter::kInvalidDischargeRateValue;
  using PowerMetricsReporter::kNoBatteryValue;
  using PowerMetricsReporter::kPluggedInDischargeRateValue;
};

// TODO(sebmarchand|etiennep): Move this to a test util file.
class FakeBatteryLevelProvider : public BatteryLevelProvider {
 public:
  explicit FakeBatteryLevelProvider(
      std::queue<BatteryLevelProvider::BatteryState>* battery_states)
      : battery_states_(battery_states) {}

  BatteryState GetBatteryState() override {
    DCHECK(!battery_states_->empty());
    BatteryLevelProvider::BatteryState state = battery_states_->front();
    battery_states_->pop();
    return state;
  }

 private:
  std::vector<BatteryInterface> GetBatteryInterfaceList() override {
    return {};
  }

  std::queue<BatteryLevelProvider::BatteryState>* battery_states_;
};

class TestProcessMonitor : public performance_monitor::ProcessMonitor {
 public:
  TestProcessMonitor() = default;
  TestProcessMonitor(const TestProcessMonitor& rhs) = delete;
  TestProcessMonitor& operator=(const TestProcessMonitor& rhs) = delete;
  ~TestProcessMonitor() override = default;

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
  PowerMetricsReporterUnitTest() = default;
  PowerMetricsReporterUnitTest(const PowerMetricsReporterUnitTest& rhs) =
      delete;
  PowerMetricsReporterUnitTest& operator=(
      const PowerMetricsReporterUnitTest& rhs) = delete;
  ~PowerMetricsReporterUnitTest() override = default;

  void SetUp() override {
    // Start with a full battery.
    battery_states_.push(BatteryLevelProvider::BatteryState{
        1, 1, 1.0, true, base::TimeTicks::Now()});
    std::unique_ptr<BatteryLevelProvider> battery_provider =
        std::make_unique<FakeBatteryLevelProvider>(&battery_states_);
    battery_provider_ = battery_provider.get();
    power_metrics_reporter_ = std::make_unique<PowerMetricsReporter>(
        data_store_.GetWeakPtr(), std::move(battery_provider));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestProcessMonitor process_monitor_;
  TestUsageScenarioDataStoreImpl data_store_;
  std::queue<BatteryLevelProvider::BatteryState> battery_states_;
  std::unique_ptr<PowerMetricsReporter> power_metrics_reporter_;
  BatteryLevelProvider* battery_provider_;
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

  const int kExpectedIntervalLengthSeconds = 120;
  task_environment_.FastForwardBy(
      base::TimeDelta::FromSeconds(kExpectedIntervalLengthSeconds));
  // Pretend that the battery has dropped by 50% in 2 minutes, for a rate of
  // 25% per minute.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});

  data_store_.SetIntervalDataToReturn(fake_interval_data);

  performance_monitor::ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = ++fake_value * 0.01;
#if defined(OS_MAC)
  fake_metrics.idle_wakeups = ++fake_value;
  fake_metrics.package_idle_wakeups = ++fake_value;
  fake_metrics.energy_impact = ++fake_value;
#endif

  process_monitor_.NotifyObserversForOnAggregatedMetricsSampled(fake_metrics);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kUptimeSecondsName,
      ukm::GetExponentialBucketMinForUserTiming(
          fake_interval_data.uptime_at_interval_end.InSeconds()));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName, 2500);
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

TEST_F(PowerMetricsReporterUnitTest, UKMsPluggedIn) {
  // Update the latest reported battery state to pretend that the system isn't
  // running on battery.
  power_metrics_reporter_->battery_state_for_testing().on_battery = false;

  const int kExpectedIntervalLengthSeconds = 120;
  task_environment_.FastForwardBy(
      base::TimeDelta::FromSeconds(kExpectedIntervalLengthSeconds));
  // Push a battery state that indicates that the system is still not running
  // on battery.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 1.0, /* on_battery - */ false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin = 42;
  data_store_.SetIntervalDataToReturn(fake_interval_data);
  process_monitor_.NotifyObserversForOnAggregatedMetricsSampled({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName,
      PowerMetricsReporterAccess::kPluggedInDischargeRateValue);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateChanges) {
  const int kExpectedIntervalLengthSeconds = 120;
  task_environment_.FastForwardBy(
      base::TimeDelta::FromSeconds(kExpectedIntervalLengthSeconds));
  // The initial battery state indicates that the system is running on battery,
  // pretends that this has changed.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 1.0, /* on_battery - */ false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin = 42;
  data_store_.SetIntervalDataToReturn(fake_interval_data);
  process_monitor_.NotifyObserversForOnAggregatedMetricsSampled({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName,
      PowerMetricsReporterAccess::kBatteryStateChangedValue);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateUnavailable) {
  const int kExpectedIntervalLengthSeconds = 120;
  task_environment_.FastForwardBy(
      base::TimeDelta::FromSeconds(kExpectedIntervalLengthSeconds));
  // A nullopt battery value indicates that the battery level is unavailable.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, base::nullopt, true, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin = 42;
  data_store_.SetIntervalDataToReturn(fake_interval_data);
  process_monitor_.NotifyObserversForOnAggregatedMetricsSampled({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName,
      PowerMetricsReporterAccess::kInvalidDischargeRateValue);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsNoBattery) {
  const int kExpectedIntervalLengthSeconds = 120;
  task_environment_.FastForwardBy(
      base::TimeDelta::FromSeconds(kExpectedIntervalLengthSeconds));
  // Indicates that the system has no battery interface.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      0, 0, 1.0, false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin = 42;
  data_store_.SetIntervalDataToReturn(fake_interval_data);
  process_monitor_.NotifyObserversForOnAggregatedMetricsSampled({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName,
      PowerMetricsReporterAccess::kNoBatteryValue);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateIncrease) {
  // Set the initial battery level at 50%.
  power_metrics_reporter_->battery_state_for_testing().charge_level = 0.5;

  const int kExpectedIntervalLengthSeconds = 120;
  task_environment_.FastForwardBy(
      base::TimeDelta::FromSeconds(kExpectedIntervalLengthSeconds));
  // Set the new battery state at 100%.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 1.0, true, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin = 42;
  data_store_.SetIntervalDataToReturn(fake_interval_data);
  process_monitor_.NotifyObserversForOnAggregatedMetricsSampled({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  // An increase in charge level is reported as an invalid discharge rate.
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName,
      PowerMetricsReporterAccess::kInvalidDischargeRateValue);
}
