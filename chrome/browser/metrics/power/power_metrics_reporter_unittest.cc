// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/performance_monitor/process_monitor.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr const char* kBatteryDischargeRateHistogramName =
    "Power.BatteryDischargeRate2";
constexpr const char* kBatteryDischargeModeHistogramName =
    "Power.BatteryDischargeMode";
constexpr base::TimeDelta kExpectedMetricsCollectionInterval =
    base::TimeDelta::FromSeconds(120);
constexpr double kTolerableTimeElapsedRatio = 0.10;
constexpr double kTolerablePositiveDrift = 1 + kTolerableTimeElapsedRatio;
constexpr double kTolerableNegativeDrift = 1 - kTolerableTimeElapsedRatio;

using UkmEntry = ukm::builders::PowerUsageScenariosIntervalData;

class PowerMetricsReporterAccess : public PowerMetricsReporter {
 public:
  using PowerMetricsReporter::BatteryDischargeMode;
  static void ReportBatteryHistograms(
      base::TimeDelta sampling_interval,
      base::TimeDelta interval_duration,
      BatteryDischargeMode discharge_mode,
      base::Optional<int64_t> discharge_rate_during_interval) {
    PowerMetricsReporter::ReportBatteryHistograms(
        sampling_interval, interval_duration, discharge_mode,
        std::move(discharge_rate_during_interval));
  }
};

// TODO(sebmarchand|etiennep): Move this to a test util file.
class FakeBatteryLevelProvider : public BatteryLevelProvider {
 public:
  explicit FakeBatteryLevelProvider(
      std::queue<BatteryLevelProvider::BatteryState>* battery_states)
      : battery_states_(battery_states) {}

  void GetBatteryState(
      base::OnceCallback<void(const BatteryState&)> callback) override {
    DCHECK(!battery_states_->empty());
    BatteryLevelProvider::BatteryState state = battery_states_->front();
    battery_states_->pop();
    std::move(callback).Run(state);
  }

 private:
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

  base::TimeDelta GetScheduledSamplingInterval() const override {
    return kExpectedMetricsCollectionInterval;
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
    base::RunLoop run_loop;
    power_metrics_reporter_ = std::make_unique<PowerMetricsReporter>(
        data_store_.AsWeakPtr(), std::move(battery_provider));
    power_metrics_reporter_->OnFirstSampleForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  void WaitForNextSample(
      const performance_monitor::ProcessMonitor::Metrics& metrics) {
    base::RunLoop run_loop;
    power_metrics_reporter_->OnNextSampleForTesting(run_loop.QuitClosure());
    process_monitor_.NotifyObserversForOnAggregatedMetricsSampled(metrics);
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestProcessMonitor process_monitor_;
  TestUsageScenarioDataStoreImpl data_store_;
  std::queue<BatteryLevelProvider::BatteryState> battery_states_;
  std::unique_ptr<PowerMetricsReporter> power_metrics_reporter_;
  BatteryLevelProvider* battery_provider_;
  base::HistogramTester histogram_tester_;
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
  fake_interval_data.time_playing_video_in_visible_tab =
      base::TimeDelta::FromSeconds(++fake_value);
  fake_interval_data.time_since_last_user_interaction_with_browser =
      base::TimeDelta::FromSeconds(++fake_value);
  fake_interval_data.time_capturing_video =
      base::TimeDelta::FromSeconds(++fake_value);
  fake_interval_data.time_playing_audio =
      base::TimeDelta::FromSeconds(++fake_value);
  fake_interval_data.longest_visible_origin_duration =
      base::TimeDelta::FromSeconds(++fake_value);

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
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

  WaitForNextSample(fake_metrics);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(entries[0]->source_id,
            fake_interval_data.source_id_for_longest_visible_origin);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kUptimeSecondsName,
      ukm::GetExponentialBucketMinForUserTiming(
          fake_interval_data.uptime_at_interval_end.InSeconds()));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName, 2500);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(
          PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kCPUTimeMsName,
      kExpectedMetricsCollectionInterval.InSeconds() * 1000 *
          fake_metrics.cpu_usage);
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
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_playing_video_full_screen_single_monitor));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kTimeWithOpenWebRTCConnectionSecondsName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_with_open_webrtc_connection));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kTimePlayingVideoInVisibleTabName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_playing_video_in_visible_tab));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kIntervalDurationSecondsName,
      kExpectedMetricsCollectionInterval.InSeconds());
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kTimeSinceInteractionWithBrowserSecondsName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_since_last_user_interaction_with_browser));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kVideoCaptureSecondsName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_capturing_video));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBrowserShuttingDownName, false);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kPlayingAudioSecondsName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_playing_audio));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kOriginVisibilityTimeSecondsName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.longest_visible_origin_duration));

  histogram_tester_.ExpectUniqueSample(kBatteryDischargeRateHistogramName, 2500,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBrowserShuttingDown) {
  UsageScenarioDataStore::IntervalData fake_interval_data = {};
  fake_interval_data.source_id_for_longest_visible_origin = 42;
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  performance_monitor::ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = 0.5;
#if defined(OS_MAC)
  fake_metrics.idle_wakeups = 42;
  fake_metrics.package_idle_wakeups = 43;
  fake_metrics.energy_impact = 44;
#endif

  {
    auto fake_shutdown = browser_shutdown::SetShutdownTypeForTesting(
        browser_shutdown::ShutdownType::kBrowserExit);
    EXPECT_TRUE(browser_shutdown::HasShutdownStarted());
    WaitForNextSample(fake_metrics);
  }

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(entries[0]->source_id, 42);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBrowserShuttingDownName, true);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsPluggedIn) {
  // Update the latest reported battery state to pretend that the system isn't
  // running on battery.
  power_metrics_reporter_->battery_state_for_testing().on_battery = false;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // Push a battery state that indicates that the system is still not running
  // on battery.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 1.0, /* on_battery - */ false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin = 42;
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(
          PowerMetricsReporterAccess::BatteryDischargeMode::kPluggedIn));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kPluggedIn, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateChanges) {
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // The initial battery state indicates that the system is running on battery,
  // pretends that this has changed.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 1.0, /* on_battery - */ false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin = 42;
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(
          PowerMetricsReporterAccess::BatteryDischargeMode::kStateChanged));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kStateChanged, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateUnavailable) {
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // A nullopt battery value indicates that the battery level is unavailable.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, base::nullopt, true, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin = 42;
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(PowerMetricsReporterAccess::BatteryDischargeMode::
                               kChargeLevelUnavailable));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kChargeLevelUnavailable,
      1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsNoBattery) {
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // Indicates that the system has no battery interface.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      0, 0, 1.0, false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin = 42;
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(
          PowerMetricsReporterAccess::BatteryDischargeMode::kNoBattery));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kNoBattery, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateIncrease) {
  // Set the initial battery level at 50%.
  power_metrics_reporter_->battery_state_for_testing().charge_level = 0.5;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // Set the new battery state at 100%.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 1.0, true, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin = 42;
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  // An increase in charge level is reported as an invalid discharge rate.
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(PowerMetricsReporterAccess::BatteryDischargeMode::
                               kInvalidDischargeRate));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kInvalidDischargeRate,
      1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsTooEarly) {
  PowerMetricsReporterAccess::ReportBatteryHistograms(
      kExpectedMetricsCollectionInterval,
      (kExpectedMetricsCollectionInterval * kTolerableNegativeDrift) -
          base::TimeDelta::FromSeconds(1),
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kInvalidInterval, 1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsEarly) {
  PowerMetricsReporterAccess::ReportBatteryHistograms(
      kExpectedMetricsCollectionInterval,
      (kExpectedMetricsCollectionInterval * kTolerableNegativeDrift) +
          base::TimeDelta::FromSeconds(1),
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  histogram_tester_.ExpectUniqueSample(kBatteryDischargeRateHistogramName, 2500,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsTooLate) {
  PowerMetricsReporterAccess::ReportBatteryHistograms(
      kExpectedMetricsCollectionInterval,
      (kExpectedMetricsCollectionInterval * kTolerablePositiveDrift) +
          base::TimeDelta::FromSeconds(1),
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kInvalidInterval, 1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsLate) {
  PowerMetricsReporterAccess::ReportBatteryHistograms(
      kExpectedMetricsCollectionInterval,
      (kExpectedMetricsCollectionInterval * kTolerablePositiveDrift) -
          base::TimeDelta::FromSeconds(1),
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  histogram_tester_.ExpectUniqueSample(kBatteryDischargeRateHistogramName, 2500,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsNoTab) {
  UsageScenarioDataStore::IntervalData fake_interval_data;

  fake_interval_data.max_tab_count = 0;
  fake_interval_data.max_visible_window_count = 0;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::kInvalidSourceId;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});

  data_store_.SetIntervalDataToReturn(fake_interval_data);

  performance_monitor::ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = 0.5;

  WaitForNextSample(fake_metrics);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(entries[0]->source_id, ukm::kInvalidSourceId);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kUptimeSecondsName,
      ukm::GetExponentialBucketMinForUserTiming(
          fake_interval_data.uptime_at_interval_end.InSeconds()));
}

TEST_F(PowerMetricsReporterUnitTest, DurationsLongerThanIntervalAreCapped) {
  UsageScenarioDataStore::IntervalData fake_interval_data;

  fake_interval_data.time_playing_video_full_screen_single_monitor =
      kExpectedMetricsCollectionInterval * 100;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  performance_monitor::ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = 0.5;
  WaitForNextSample(fake_metrics);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(entries[0]->source_id, ukm::kInvalidSourceId);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kFullscreenVideoSingleMonitorSecondsName,
      // Every value greater than |kExpectedMetricsCollectionInterval| should
      // fall in the same overflow bucket.
      PowerMetricsReporter::GetBucketForSampleForTesting(
          kExpectedMetricsCollectionInterval * 2));
}
