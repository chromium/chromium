// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/metrics/power/process_monitor.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/metrics/power/coalition_resource_usage_provider_test_util_mac.h"
#include "components/power_metrics/resource_coalition_mac.h"
#endif

namespace {

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
constexpr const char* kBatteryDischargeRateHistogramName =
    "Power.BatteryDischargeRate2";
constexpr const char* kBatteryDischargeModeHistogramName =
    "Power.BatteryDischargeMode";

constexpr double kTolerableTimeElapsedRatio = 0.10;
constexpr double kTolerablePositiveDrift = 1 + kTolerableTimeElapsedRatio;
constexpr double kTolerableNegativeDrift = 1 - kTolerableTimeElapsedRatio;
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

constexpr base::TimeDelta kExpectedMetricsCollectionInterval = base::Minutes(2);

ProcessMonitor::Metrics GetFakeProcessMetrics() {
  ProcessMonitor::Metrics metrics;
  metrics.cpu_usage = 5;
  return metrics;
}

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

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
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
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

class TestProcessMonitor : public ProcessMonitor {
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
#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
    // Start with a half-full battery
    battery_states_.push(BatteryLevelProvider::BatteryState{
        1, 1, 0.5, true, base::TimeTicks::Now()});
    auto battery_provider =
        std::make_unique<FakeBatteryLevelProvider>(&battery_states_);
    battery_provider_ = battery_provider.get();
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

#if BUILDFLAG(IS_MAC)
    auto coalition_resource_usage_provider =
        std::make_unique<TestCoalitionResourceUsageProvider>();
    // Ensure that coalition resource usage is available from Init().
    coalition_resource_usage_provider->SetCoalitionResourceUsage(
        std::make_unique<coalition_resource_usage>());
    coalition_resource_usage_provider_ =
        coalition_resource_usage_provider.get();
#endif  // BUILDFLAG(IS_MAC)

    power_metrics_reporter_ = std::make_unique<PowerMetricsReporter>(
        &process_monitor_, &short_data_store_, &long_data_store_
#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
        ,
        std::move(battery_provider)
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()
#if BUILDFLAG(IS_MAC)
            ,
        std::move(coalition_resource_usage_provider)
#endif  // BUILDFLAG(IS_MAC)
    );

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
    base::RunLoop run_loop;
    power_metrics_reporter_->OnFirstSampleForTesting(run_loop.QuitClosure());
    run_loop.Run();
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()
  }

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
  void WaitForNextBatterySample(const ProcessMonitor::Metrics& metrics) {
    base::RunLoop run_loop;
    power_metrics_reporter_->OnNextSampleForTesting(run_loop.QuitClosure());
    process_monitor_.NotifyObserversForOnAggregatedMetricsSampled(metrics);
    run_loop.Run();
  }
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestProcessMonitor process_monitor_;
  TestUsageScenarioDataStoreImpl short_data_store_;
  TestUsageScenarioDataStoreImpl long_data_store_;

  base::HistogramTester histogram_tester_;

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

  std::queue<BatteryLevelProvider::BatteryState> battery_states_;
  raw_ptr<BatteryLevelProvider> battery_provider_;
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

#if BUILDFLAG(IS_MAC)
  raw_ptr<TestCoalitionResourceUsageProvider>
      coalition_resource_usage_provider_;
#endif  // BUILDFLAG(IS_MAC)

  std::unique_ptr<PowerMetricsReporter> power_metrics_reporter_;
};

}  // namespace

TEST_F(PowerMetricsReporterUnitTest, LongIntervalHistograms) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::Seconds(1);
  long_data_store_.SetIntervalDataToReturn(interval_data);

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.30, true, base::TimeTicks::Now()});
  WaitForNextBatterySample(GetFakeProcessMetrics());
#else
  process_monitor_.NotifyObserversForOnAggregatedMetricsSampled(
      GetFakeProcessMetrics());
#endif

  const char* kScenarioSuffix = ".VideoCapture";
  const std::vector<const char*> suffixes({"", kScenarioSuffix});
  ExpectHistogramSamples(&histogram_tester_, suffixes,
                         {{"PerformanceMonitor.AverageCPU2.Total", 500}});
}

#if BUILDFLAG(IS_MAC)
TEST_F(PowerMetricsReporterUnitTest, ResourceCoalitionHistograms_EndToEnd) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::Seconds(1);
  long_data_store_.SetIntervalDataToReturn(interval_data);

  auto cru1 = std::make_unique<coalition_resource_usage>();
  cru1->cpu_time = base::Seconds(5).InNanoseconds();
  coalition_resource_usage_provider_->SetCoalitionResourceUsage(
      std::move(cru1));
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval -
                                  PowerMetricsReporter::kShortIntervalDuration);

  auto cru2 = std::make_unique<coalition_resource_usage>();
  cru2->cpu_time = base::Seconds(6).InNanoseconds();
  coalition_resource_usage_provider_->SetCoalitionResourceUsage(
      std::move(cru2));
  task_environment_.FastForwardBy(PowerMetricsReporter::kShortIntervalDuration);

  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.30, true, base::TimeTicks::Now()});
  ProcessMonitor::Metrics aggregated_process_metrics = {};
  WaitForNextBatterySample(aggregated_process_metrics);

  const char* kScenarioSuffix = ".VideoCapture";
  const std::vector<const char*> suffixes({"", kScenarioSuffix});
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.ResourceCoalition.CPUTime2", 500}});
}
#endif

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsTooEarly) {
  // Pretend that the battery has dropped by 2%.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.48, true, base::TimeTicks::Now()});

  const base::TimeDelta kTooEarly =
      kExpectedMetricsCollectionInterval * kTolerableNegativeDrift -
      base::Microseconds(1);
  task_environment_.FastForwardBy(kTooEarly);

  ProcessMonitor::Metrics aggregated_process_metrics = {};
  WaitForNextBatterySample(aggregated_process_metrics);

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kInvalidInterval,
                                       1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsEarly) {
  // Pretend that the battery has dropped by 2%.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.48, true, base::TimeTicks::Now()});

  const base::TimeDelta kEarly =
      kExpectedMetricsCollectionInterval * kTolerableNegativeDrift +
      base::Microseconds(1);
  task_environment_.FastForwardBy(kEarly);

  ProcessMonitor::Metrics aggregated_process_metrics = {};
  WaitForNextBatterySample(aggregated_process_metrics);

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 1);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kDischarging, 1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsTooLate) {
  // Pretend that the battery has dropped by 2%.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.48, true, base::TimeTicks::Now()});

  const base::TimeDelta kTooLate =
      kExpectedMetricsCollectionInterval * kTolerablePositiveDrift +
      base::Microseconds(1);
  task_environment_.FastForwardBy(kTooLate);

  ProcessMonitor::Metrics aggregated_process_metrics = {};
  WaitForNextBatterySample(aggregated_process_metrics);

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kInvalidInterval,
                                       1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsLate) {
  // Pretend that the battery has dropped by 2%.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.48, true, base::TimeTicks::Now()});

  const base::TimeDelta kLate =
      kExpectedMetricsCollectionInterval * kTolerablePositiveDrift -
      base::Microseconds(1);
  task_environment_.FastForwardBy(kLate);

  ProcessMonitor::Metrics aggregated_process_metrics = {};
  WaitForNextBatterySample(aggregated_process_metrics);

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 1);
  histogram_tester_.ExpectUniqueSample(kBatteryDischargeModeHistogramName,
                                       BatteryDischargeMode::kDischarging, 1);
}

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

  ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = ++fake_value * 0.01;
#if BUILDFLAG(IS_MAC)
  fake_metrics.idle_wakeups = ++fake_value;
  fake_metrics.package_idle_wakeups = ++fake_value;
  fake_metrics.energy_impact = ++fake_value;
#endif

  WaitForNextBatterySample(fake_metrics);

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

  ProcessMonitor::Metrics fake_metrics = {};
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
    WaitForNextBatterySample(fake_metrics);
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

  WaitForNextBatterySample({});

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

  WaitForNextBatterySample({});

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

  WaitForNextBatterySample({});

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

  WaitForNextBatterySample({});

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

  WaitForNextBatterySample({});

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

  WaitForNextBatterySample({});

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

  WaitForNextBatterySample(GetFakeProcessMetrics());

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

  WaitForNextBatterySample(GetFakeProcessMetrics());

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
  ProcessMonitor::Metrics fake_metrics = {};
  WaitForNextBatterySample(fake_metrics);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kDeviceSleptDuringIntervalName, true);
}
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

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
  ProcessMonitor::Metrics aggregated_process_metrics = {};
  WaitForNextBatterySample(aggregated_process_metrics);

  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", 6000, 1);
}
#endif  // BUILDFLAG(IS_MAC)
