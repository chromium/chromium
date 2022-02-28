// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
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

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/metrics/power/coalition_resource_usage_provider_test_util_mac.h"
#include "components/power_metrics/resource_coalition_mac.h"
#endif

namespace {

constexpr const char* kBatteryDischargeRateHistogramName =
    "Power.BatteryDischargeRate2";
constexpr const char* kBatteryDischargeModeHistogramName =
    "Power.BatteryDischargeMode";

constexpr base::TimeDelta kExpectedMetricsCollectionInterval =
    base::Seconds(120);
constexpr double kTolerableTimeElapsedRatio = 0.10;
constexpr double kTolerablePositiveDrift = 1 + kTolerableTimeElapsedRatio;
constexpr double kTolerableNegativeDrift = 1 - kTolerableTimeElapsedRatio;

performance_monitor::ProcessMonitor::Metrics GetFakeProcessMetrics() {
  performance_monitor::ProcessMonitor::Metrics metrics;
  metrics.cpu_usage = 5;
  return metrics;
}

#if BUILDFLAG(IS_MAC)
power_metrics::CoalitionResourceUsageRate GetFakeResourceUsageRate() {
  power_metrics::CoalitionResourceUsageRate rate;
  rate.cpu_time_per_second = 0.5;
  rate.interrupt_wakeups_per_second = 10;
  rate.platform_idle_wakeups_per_second = 11;
  rate.bytesread_per_second = 12;
  rate.byteswritten_per_second = 13;
  rate.gpu_time_per_second = 0.6;
  rate.energy_impact_per_second = 15;
  rate.power_nw = 1000000;

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i)
    rate.qos_time_per_second[i] = 0.1 * i;

  return rate;
}
#endif  // BUILDFLAG(IS_MAC)

struct HistogramSampleExpectation {
  std::string histogram_name_prefix;
  base::Histogram::Sample sample;
};

// For each histogram named after the combination of prefixes from
// `expectations` and suffixes from `suffixes`, verifies that there is a unique
// sample `expectation.sample`.
void ExpectHistogramSamples(
    base::HistogramTester* histogram_tester,
    const std::vector<const char*>& suffixes,
    const std::vector<HistogramSampleExpectation>& expectations) {
  for (const char* suffix : suffixes) {
    for (const auto& expectation : expectations) {
      std::string histogram_name =
          base::StrCat({expectation.histogram_name_prefix, suffix});
      SCOPED_TRACE(histogram_name);
      histogram_tester->ExpectUniqueSample(histogram_name, expectation.sample,
                                           1);
    }
  }
}

using UkmEntry = ukm::builders::PowerUsageScenariosIntervalData;

class PowerMetricsReporterAccess : public PowerMetricsReporter {
 public:
  using PowerMetricsReporter::PowerMetricsReporter;

  // Expose members of PowerMetricsReporter publicly on
  // PowerMetricsReporterAccess.
  using PowerMetricsReporter::BatteryDischarge;
  using PowerMetricsReporter::BatteryDischargeMode;
  using PowerMetricsReporter::ReportBatteryHistograms;
  using PowerMetricsReporter::ReportLongIntervalHistograms;
#if BUILDFLAG(IS_MAC)
  using PowerMetricsReporter::ReportResourceCoalitionHistograms;
  using PowerMetricsReporter::ReportShortIntervalHistograms;
#endif  // BUILDFLAG(IS_MAC)
};

using BatteryDischargeMode = PowerMetricsReporterAccess::BatteryDischargeMode;
using BatteryDischarge = PowerMetricsReporterAccess::BatteryDischarge;

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
  raw_ptr<std::queue<BatteryLevelProvider::BatteryState>> battery_states_;
};

class TestProcessMonitor : public performance_monitor::ProcessMonitor {
 public:
  TestProcessMonitor() = default;
  TestProcessMonitor(const TestProcessMonitor& rhs) = delete;
  TestProcessMonitor& operator=(const TestProcessMonitor& rhs) = delete;
  ~TestProcessMonitor() override = default;

  // Call OnAggregatedMetricsSampled for all the observers with |metrics|
  // as an argument.
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
    // Start with a half-full battery.
    battery_states_.push(BatteryLevelProvider::BatteryState{
        1, 1, 0.5, true, base::TimeTicks::Now()});
    auto battery_provider =
        std::make_unique<FakeBatteryLevelProvider>(&battery_states_);
    battery_provider_ = battery_provider.get();
#if BUILDFLAG(IS_MAC)
    auto coalition_resource_usage_provider =
        std::make_unique<TestCoalitionResourceUsageProvider>();
    // Ensure that coalition resource usage is available from Init().
    coalition_resource_usage_provider->SetCoalitionResourceUsage(
        std::make_unique<coalition_resource_usage>());
    coalition_resource_usage_provider_ =
        coalition_resource_usage_provider.get();
#endif  // BUILDFLAG(IS_MAC)
    base::RunLoop run_loop;
    power_metrics_reporter_ = std::make_unique<PowerMetricsReporter>(
        &short_data_store_, &long_data_store_, std::move(battery_provider)
#if BUILDFLAG(IS_MAC)
                                                   ,
        std::move(coalition_resource_usage_provider)
#endif  // BUILDFLAG(IS_MAC)
    );
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
  TestUsageScenarioDataStoreImpl short_data_store_;
  TestUsageScenarioDataStoreImpl long_data_store_;
  std::queue<BatteryLevelProvider::BatteryState> battery_states_;
  std::unique_ptr<PowerMetricsReporter> power_metrics_reporter_;
  raw_ptr<BatteryLevelProvider> battery_provider_;
#if BUILDFLAG(IS_MAC)
  raw_ptr<TestCoalitionResourceUsageProvider>
      coalition_resource_usage_provider_;
#endif  // BUILDFLAG(IS_MAC)
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

}  // namespace

TEST_F(PowerMetricsReporterUnitTest, UKMs) {
  UsageScenarioDataStore::IntervalData fake_interval_data;

  int fake_value = 42;
  fake_interval_data.uptime_at_interval_end = base::Hours(++fake_value);
  fake_interval_data.max_tab_count = ++fake_value;
  fake_interval_data.max_visible_window_count = ++fake_value;
  fake_interval_data.top_level_navigation_count = ++fake_value;
  fake_interval_data.tabs_closed_during_interval = ++fake_value;
  fake_interval_data.user_interaction_count = ++fake_value;
  fake_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(++fake_value);
  fake_interval_data.time_with_open_webrtc_connection =
      base::Seconds(++fake_value);
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  fake_interval_data.source_id_for_longest_visible_origin_duration =
      base::Seconds(++fake_value);
  fake_interval_data.time_playing_video_in_visible_tab =
      base::Seconds(++fake_value);
  fake_interval_data.time_since_last_user_interaction_with_browser =
      base::Seconds(++fake_value);
  fake_interval_data.time_capturing_video = base::Seconds(++fake_value);
  fake_interval_data.time_playing_audio = base::Seconds(++fake_value);
  fake_interval_data.longest_visible_origin_duration =
      base::Seconds(++fake_value);
  fake_interval_data.sleep_events = 0;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // Pretend that the battery has dropped by 20% in 2 minutes, for a rate of
  // 10% per minute.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.30, true, base::TimeTicks::Now()});

  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  performance_monitor::ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = ++fake_value * 0.01;
#if BUILDFLAG(IS_MAC)
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
      entries[0], UkmEntry::kBatteryDischargeRateName, 1000);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kDischarging));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kCPUTimeMsName,
      kExpectedMetricsCollectionInterval.InSeconds() * 1000 *
          fake_metrics.cpu_usage);
#if BUILDFLAG(IS_MAC)
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
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kDeviceSleptDuringIntervalName, false);

  histogram_tester_.ExpectUniqueSample(kBatteryDischargeRateHistogramName, 2500,
                                       1);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kDischarging, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBrowserShuttingDown) {
  const ukm::SourceId kTestSourceId =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  UsageScenarioDataStore::IntervalData fake_interval_data = {};
  fake_interval_data.source_id_for_longest_visible_origin = kTestSourceId;
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  performance_monitor::ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = 0.5;
#if BUILDFLAG(IS_MAC)
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

  EXPECT_EQ(entries[0]->source_id, kTestSourceId);
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
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kPluggedIn));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kPluggedIn, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateChanges) {
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // The initial battery state indicates that the system is running on battery,
  // pretends that this has changed.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 1.0, /* on_battery - */ false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kStateChanged));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kStateChanged, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateUnavailable) {
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // A nullopt battery value indicates that the battery level is unavailable.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, absl::nullopt, true, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kChargeLevelUnavailable));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      BatteryDischargeMode::kChargeLevelUnavailable, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsNoBattery) {
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // Indicates that the system has no battery interface.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      0, 0, 1.0, false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kNoBattery));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kNoBattery, 1);
}

#if BUILDFLAG(IS_MAC)
// Tests that on MacOS, a full |charge_level| while not plugged does not result
// in a kDischarging value emitted. See https://crbug.com/1249830.
TEST_F(PowerMetricsReporterUnitTest, UKMsMacFullyCharged) {
  // Set the initial battery level at 100%.
  power_metrics_reporter_->battery_state_for_testing().charge_level = 1.0;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      0, 1, 1.0, true, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kMacFullyCharged));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kMacFullyCharged,
                                       1);
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateIncrease) {
  // Set the initial battery level at 50%.
  power_metrics_reporter_->battery_state_for_testing().charge_level = 0.5;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // Set the new battery state at 75%.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.75, true, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  // An increase in charge level is reported as an invalid discharge rate.
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kBatteryLevelIncreased));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      BatteryDischargeMode::kBatteryLevelIncreased, 1);
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_ZeroWindow) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 0;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes({"", ".ZeroWindow"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* and ResourceCoalition.* histograms is recorded
  // correctly.
}

TEST_F(PowerMetricsReporterUnitTest,
       SuffixedHistograms_AllTabsHidden_VideoCapture) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 0;
  interval_data.time_capturing_video = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes({"", ".AllTabsHidden_VideoCapture"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_AllTabsHidden_Audio) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 0;
  interval_data.time_capturing_video = base::Seconds(0);
  interval_data.time_playing_audio = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes({"", ".AllTabsHidden_Audio"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest,
       SuffixedHistograms_AllTabsHidden_NoVideoCaptureOrAudio) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 0;
  interval_data.time_capturing_video = base::Seconds(0);
  interval_data.time_playing_audio = base::Seconds(0);
  // Values below should be ignored.
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes(
      {"", ".AllTabsHidden_NoVideoCaptureOrAudio"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* and ResourceCoalition.* histograms is recorded
  // correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_VideoCapture) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes({"", ".VideoCapture"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* and ResourceCoalition.* histograms is recorded
  // correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_FullscreenVideo) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes({"", ".FullscreenVideo"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* and ResourceCoalition.* histograms is recorded
  // correctly.
}

TEST_F(PowerMetricsReporterUnitTest,
       SuffixedHistograms_EmbeddedVideo_NoNavigation) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.top_level_navigation_count = 0;
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes({"", ".EmbeddedVideo_NoNavigation"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* and ResourceCoalition.* histograms is recorded
  // correctly.
}

TEST_F(PowerMetricsReporterUnitTest,
       SuffixedHistograms_EmbeddedVideo_WithNavigation) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.top_level_navigation_count = 1;
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes(
      {"", ".EmbeddedVideo_WithNavigation"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* and ResourceCoalition.* histograms is recorded
  // correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_Audio) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::Seconds(1);
  // Values below should be ignored.
  interval_data.user_interaction_count = 1;
  interval_data.top_level_navigation_count = 1;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes({"", ".Audio"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* and ResourceCoalition.* histograms is recorded
  // correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_Navigation) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::TimeDelta();
  interval_data.top_level_navigation_count = 1;
  // Values below should be ignored.
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes({"", ".Navigation"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* and ResourceCoalition.* histograms is recorded
  // correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_Interaction) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::TimeDelta();
  interval_data.top_level_navigation_count = 0;
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes({"", ".Interaction"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* and ResourceCoalition.* histograms is recorded
  // correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_Passive) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::TimeDelta();
  interval_data.top_level_navigation_count = 0;
  interval_data.user_interaction_count = 0;

  PowerMetricsReporterAccess::ReportLongIntervalHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      BatteryDischarge { BatteryDischargeMode::kDischarging, 2500 }
#if BUILDFLAG(IS_MAC)
      ,
      GetFakeResourceUsageRate()
#endif  // BUILDFLAG(IS_MAC)
  );

  const std::vector<const char*> suffixes({"", ".Passive"});
  ExpectHistogramSamples(&histogram_tester_, suffixes, {
    {"Power.BatteryDischargeRate2", 2500},
        {"Power.BatteryDischargeMode", static_cast<base::Histogram::Sample>(
                                           BatteryDischargeMode::kDischarging)},
    {
      "PerformanceMonitor.AverageCPU2.Total", 500
    }
#if BUILDFLAG(IS_MAC)
    , { "PerformanceMonitor.ResourceCoalition.CPUTime2", 5000 }
#endif  // BUILDFLAG(IS_MAC)
  });

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* and ResourceCoalition.* histograms is recorded
  // correctly.
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsTooEarly) {
  UsageScenarioDataStore::IntervalData interval_data;

  PowerMetricsReporterAccess::ReportBatteryHistograms(
      (kExpectedMetricsCollectionInterval * kTolerableNegativeDrift) -
          base::Seconds(1),
      BatteryDischarge{BatteryDischargeMode::kDischarging, 2500},
      PowerMetricsReporter::GetLongIntervalSuffixesForTesting(interval_data));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kInvalidInterval,
                                       1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsEarly) {
  UsageScenarioDataStore::IntervalData interval_data;

  PowerMetricsReporterAccess::ReportBatteryHistograms(
      (kExpectedMetricsCollectionInterval * kTolerableNegativeDrift) +
          base::Seconds(1),
      BatteryDischarge{BatteryDischargeMode::kDischarging, 2500},
      PowerMetricsReporter::GetLongIntervalSuffixesForTesting(interval_data));

  histogram_tester_.ExpectUniqueSample(kBatteryDischargeRateHistogramName, 2500,
                                       1);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kDischarging, 1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsTooLate) {
  UsageScenarioDataStore::IntervalData interval_data;

  PowerMetricsReporterAccess::ReportBatteryHistograms(
      (kExpectedMetricsCollectionInterval * kTolerablePositiveDrift) +
          base::Seconds(1),
      BatteryDischarge{BatteryDischargeMode::kDischarging, 2500},
      PowerMetricsReporter::GetLongIntervalSuffixesForTesting(interval_data));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kInvalidInterval,
                                       1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsLate) {
  UsageScenarioDataStore::IntervalData interval_data;

  PowerMetricsReporterAccess::ReportBatteryHistograms(
      (kExpectedMetricsCollectionInterval * kTolerablePositiveDrift) -
          base::Seconds(1),
      BatteryDischarge{BatteryDischargeMode::kDischarging, 2500},
      PowerMetricsReporter::GetLongIntervalSuffixesForTesting(interval_data));

  histogram_tester_.ExpectUniqueSample(kBatteryDischargeRateHistogramName, 2500,
                                       1);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kDischarging, 1);
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

  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample(GetFakeProcessMetrics());

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
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample(GetFakeProcessMetrics());

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

TEST_F(PowerMetricsReporterUnitTest, UKMsWithSleepEvent) {
  UsageScenarioDataStore::IntervalData fake_interval_data = {};
  fake_interval_data.sleep_events = 1;
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);
  performance_monitor::ProcessMonitor::Metrics fake_metrics = {};
  WaitForNextSample(fake_metrics);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kDeviceSleptDuringIntervalName, true);
}

#if BUILDFLAG(IS_MAC)
// Verify that "_10sec" resource coalition histograms are recorded when time
// advances and resource coalition data is available.
TEST_F(PowerMetricsReporterUnitTest, ShortIntervalHistograms_EndToEnd) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 0;
  short_data_store_.SetIntervalDataToReturn(interval_data);

  auto cru1 = std::make_unique<coalition_resource_usage>();
  cru1->cpu_time = base::Seconds(4).InNanoseconds();
  coalition_resource_usage_provider_->SetCoalitionResourceUsage(
      std::move(cru1));
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval -
                                  PowerMetricsReporter::kShortIntervalDuration);

  auto cru2 = std::make_unique<coalition_resource_usage>();
  cru2->cpu_time = base::Seconds(10).InNanoseconds();
  coalition_resource_usage_provider_->SetCoalitionResourceUsage(
      std::move(cru2));
  task_environment_.FastForwardBy(PowerMetricsReporter::kShortIntervalDuration);

  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.30, true, base::TimeTicks::Now()});
  performance_monitor::ProcessMonitor::Metrics aggregated_process_metrics = {};
  WaitForNextSample(aggregated_process_metrics);

  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 6000, 1);
}

TEST_F(PowerMetricsReporterUnitTest, ShortIntervalHistograms_ZeroWindow) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 0;

  UsageScenarioDataStore::IntervalData long_interval_data;
  long_interval_data.max_tab_count = 0;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", ".ZeroWindow"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest,
       ShortIntervalHistograms_ZeroWindow_Recent) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 0;

  UsageScenarioDataStore::IntervalData long_interval_data;
  long_interval_data.max_tab_count = 1;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", ".ZeroWindow_Recent"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest,
       ShortIntervalHistograms_AllTabsHidden_VideoCapture) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 0;
  short_interval_data.time_capturing_video = base::Seconds(1);
  // Values below should be ignored.
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.time_playing_audio = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;
  long_interval_data.max_visible_window_count = 1;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", ".AllTabsHidden_VideoCapture"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest,
       ShortIntervalHistograms_AllTabsHidden_Audio) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 0;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(1);
  // Values below should be ignored.
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;
  long_interval_data.max_visible_window_count = 1;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", ".AllTabsHidden_Audio"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest,
       ShortIntervalHistograms_AllTabsHidden_NoVideoCaptureOrAudio) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 0;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(0);
  // Values below should be ignored.
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes(
      {"", ".AllTabsHidden_NoVideoCaptureOrAudio"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest,
       ShortIntervalHistograms_AllTabsHidden_NoVideoCaptureOrAudio_Recent) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 0;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(0);
  // Values below should be ignored.
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;
  long_interval_data.max_visible_window_count = 1;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes(
      {"", ".AllTabsHidden_NoVideoCaptureOrAudio_Recent"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest, ShortIntervalHistograms_VideoCapture) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(1);
  // Values below should be ignored.
  short_interval_data.time_playing_audio = base::Seconds(1);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", ".VideoCapture"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest, ShortIntervalHistograms_FullscreenVideo) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  // Values below should be ignored.
  short_interval_data.time_playing_audio = base::Seconds(1);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", ".FullscreenVideo"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest,
       ShortIntervalHistograms_EmbeddedVideo_NoNavigation) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 0;
  // Values below should be ignored.
  short_interval_data.time_playing_audio = base::Seconds(1);
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", ".EmbeddedVideo_NoNavigation"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest,
       ShortIntervalHistograms_EmbeddedVideo_WithNavigation) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  short_interval_data.top_level_navigation_count = 1;
  // Values below should be ignored.
  short_interval_data.time_playing_audio = base::Seconds(1);
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes(
      {"", ".EmbeddedVideo_WithNavigation"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest, ShortIntervalHistograms_Audio) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(1);
  // Values below should be ignored.
  short_interval_data.top_level_navigation_count = 1;
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", ".Audio"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest, ShortIntervalHistograms_Navigation) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(0);
  short_interval_data.top_level_navigation_count = 1;
  // Values below should be ignored.
  short_interval_data.user_interaction_count = 1;

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", ".Navigation"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest, ShortIntervalHistograms_Interaction) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(0);
  short_interval_data.top_level_navigation_count = 0;
  short_interval_data.user_interaction_count = 1;
  // Values below should be ignored.

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", ".Interaction"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest, ShortIntervalHistograms_Passive) {
  UsageScenarioDataStore::IntervalData short_interval_data;
  short_interval_data.max_tab_count = 1;
  short_interval_data.max_visible_window_count = 1;
  short_interval_data.time_capturing_video = base::Seconds(0);
  short_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(0);
  short_interval_data.time_playing_video_in_visible_tab = base::Seconds(0);
  short_interval_data.time_playing_audio = base::Seconds(0);
  short_interval_data.top_level_navigation_count = 0;
  short_interval_data.user_interaction_count = 0;
  // Values below should be ignored.

  UsageScenarioDataStore::IntervalData long_interval_data = short_interval_data;

  PowerMetricsReporterAccess::ReportShortIntervalHistograms(
      short_interval_data, long_interval_data, GetFakeResourceUsageRate());

  const std::vector<const char*> suffixes({"", ".Passive"});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 5000}});
}

TEST_F(PowerMetricsReporterUnitTest, ResourceCoalitionHistograms) {
  base::HistogramTester histogram_tester;

  const std::vector<const char*> suffixes = {"", ".Foo", ".Bar"};
  PowerMetricsReporterAccess::ReportResourceCoalitionHistograms(
      GetFakeResourceUsageRate(), suffixes);

  ExpectHistogramSamples(
      &histogram_tester, suffixes,
      {// These histograms reports the CPU/GPU times as a percentage of
       // time with a permyriad granularity, 10% (0.1) will be represented
       // as 1000.
       {"PerformanceMonitor.ResourceCoalition.CPUTime2", 5000},
       {"PerformanceMonitor.ResourceCoalition.GPUTime2", 6000},
       // These histograms report counts with a millievent/second
       // granularity.
       {"PerformanceMonitor.ResourceCoalition.InterruptWakeupsPerSecond",
        10000},
       {"PerformanceMonitor.ResourceCoalition."
        "PlatformIdleWakeupsPerSecond",
        11000},
       {"PerformanceMonitor.ResourceCoalition.BytesReadPerSecond2", 12},
       {"PerformanceMonitor.ResourceCoalition.BytesWrittenPerSecond2", 13},
       // EI is reported in centi-EI so the data needs to be multiplied by
       // 100.0.
       {"PerformanceMonitor.ResourceCoalition.EnergyImpact", 1500},
       // The QoS histograms also reports the CPU times as a percentage of
       // time with a permyriad granularity.
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.Default", 0},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.Maintenance", 1000},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.Background", 2000},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.Utility", 3000},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.Legacy", 4000},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.UserInitiated", 5000},
       {"PerformanceMonitor.ResourceCoalition.QoSLevel.UserInteractive",
        6000}});

  if (base::mac::GetCPUType() == base::mac::CPUType::kArm) {
    ExpectHistogramSamples(
        &histogram_tester, suffixes,
        {// Power is reported in milliwatts (mj/s), the data
         // is in nj/s so it has to be divided by 1000000.
         {"PerformanceMonitor.ResourceCoalition.Power2", 1}});
  } else {
    histogram_tester.ExpectTotalCount(
        "PerformanceMonitor.ResourceCoalition.Power2", 0);
  }
}

// Verify that no energy impact histogram is reported when
// `CoalitionResourceUsageRate::energy_impact_per_second` is nullopt.
TEST_F(PowerMetricsReporterUnitTest,
       ReportResourceCoalitionHistograms_NoEnergyImpact) {
  base::HistogramTester histogram_tester;
  power_metrics::CoalitionResourceUsageRate rate = GetFakeResourceUsageRate();
  rate.energy_impact_per_second.reset();

  std::vector<const char*> suffixes = {"", ".Foo"};
  PowerMetricsReporterAccess::ReportResourceCoalitionHistograms(rate, suffixes);

  histogram_tester.ExpectTotalCount(
      "PerformanceMonitor.ResourceCoalition.EnergyImpact", 0);
  histogram_tester.ExpectTotalCount(
      "PerformanceMonitor.ResourceCoalition.EnergyImpact.Foo", 0);
}
#endif  // BUILDFLAG(IS_MAC)
