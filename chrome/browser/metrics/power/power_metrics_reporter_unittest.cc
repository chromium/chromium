// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/metrics/power/power_metrics_constants.h"
#include "chrome/browser/metrics/power/process_monitor.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::BatteryLevelProvider::BatteryState MakeBatteryDischargingState(
    int battery_percent) {
  return base::BatteryLevelProvider::BatteryState{
      .battery_count = 1,
      .is_external_power_connected = false,
      .current_capacity = battery_percent,
      .full_charged_capacity = 100,
      .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      .capture_time = base::TimeTicks::Now()};
}

ProcessMonitor::Metrics GetFakeProcessMetrics(bool with_cpu_usage = true) {
  ProcessMonitor::Metrics metrics;
  if (with_cpu_usage) {
    metrics.cpu_usage = 5;
  }
  return metrics;
}

struct HistogramSampleExpectation {
  std::string histogram_name_prefix;
  std::optional<base::Histogram::Sample> sample;
};

// For each histogram named after the combination of prefixes from
// `expectations` and suffixes from `suffixes`, verifies that there is a unique
// sample `expectation.sample`, or no sample if `expectation.sample` is nullopt.
void ExpectHistogramSamples(
    base::HistogramTester* histogram_tester,
    const std::vector<const char*>& suffixes,
    const std::vector<HistogramSampleExpectation>& expectations) {
  for (const char* suffix : suffixes) {
    for (const auto& expectation : expectations) {
      std::string histogram_name =
          base::StrCat({expectation.histogram_name_prefix, suffix});
      SCOPED_TRACE(histogram_name);
      if (expectation.sample.has_value()) {
        histogram_tester->ExpectUniqueSample(histogram_name,
                                             expectation.sample.value(), 1);
      } else {
        histogram_tester->ExpectTotalCount(histogram_name, 0);
      }
    }
  }
}

using UkmEntry = ukm::builders::PowerUsageScenariosIntervalData;

class FakeBatteryLevelProvider : public base::BatteryLevelProvider {
 public:
  explicit FakeBatteryLevelProvider(
      std::queue<std::optional<base::BatteryLevelProvider::BatteryState>>*
          battery_states)
      : battery_states_(battery_states) {}

  void GetBatteryState(
      base::OnceCallback<void(const std::optional<BatteryState>&)> callback)
      override {
    DCHECK(!battery_states_->empty());
    auto state = battery_states_->front();
    battery_states_->pop();
    std::move(callback).Run(state);
  }

 private:
  raw_ptr<std::queue<std::optional<base::BatteryLevelProvider::BatteryState>>>
      battery_states_;
};

class TestProcessMonitor : public ProcessMonitor {
 public:
  TestProcessMonitor() = default;
  TestProcessMonitor(const TestProcessMonitor& rhs) = delete;
  TestProcessMonitor& operator=(const TestProcessMonitor& rhs) = delete;
  ~TestProcessMonitor() override = default;

  void SetMetricsToReturn(const Metrics& metrics) { metrics_ = metrics; }

  void SampleAllProcesses(Observer* observer) override {
    if (!periodic_sampling_enabled_)
      return;

    observer->OnAggregatedMetricsSampled(metrics_);
  }

  // Used to force a call to `OnAggregatedMetricsSampled` at any time, instead
  // of being called automatically by the interval timer.
  void ForceSampleAllProcessesIn(Observer* observer,
                                 base::test::TaskEnvironment* task_environment,
                                 base::TimeDelta time_delta) {
    periodic_sampling_enabled_ = false;

    task_environment->FastForwardBy(time_delta);

    observer->OnAggregatedMetricsSampled(metrics_);
  }

 private:
  Metrics metrics_;

  // Indicates if the periodic sampling driven by the interval timer is enabled.
  // Only disabled when `ForceSampleAllProcessesIn` is used.
  bool periodic_sampling_enabled_ = true;
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
class PowerMetricsReporterUnitTestBase : public testing::Test {
 public:
  PowerMetricsReporterUnitTestBase() = default;
  PowerMetricsReporterUnitTestBase(
      const PowerMetricsReporterUnitTestBase& rhs) = delete;
  PowerMetricsReporterUnitTestBase& operator=(
      const PowerMetricsReporterUnitTestBase& rhs) = delete;
  ~PowerMetricsReporterUnitTestBase() override = default;

  void SetUp() override {
    auto battery_provider = CreateBatteryLevelProvider();
    battery_provider_ = battery_provider.get();

    power_metrics_reporter_ = std::make_unique<PowerMetricsReporter>(
        &process_monitor_, &long_data_store_, std::move(battery_provider));

    // Ensure the first battery state is sampled.
    task_environment_.RunUntilIdle();
  }

 protected:
  virtual std::unique_ptr<base::BatteryLevelProvider>
  CreateBatteryLevelProvider() = 0;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestProcessMonitor process_monitor_;
  TestUsageScenarioDataStoreImpl long_data_store_;

  base::HistogramTester histogram_tester_;

  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

  raw_ptr<base::BatteryLevelProvider, DanglingUntriaged> battery_provider_;

  std::unique_ptr<PowerMetricsReporter> power_metrics_reporter_;
};

class PowerMetricsReporterUnitTest : public PowerMetricsReporterUnitTestBase {
 public:
  ~PowerMetricsReporterUnitTest() override = default;

  std::unique_ptr<base::BatteryLevelProvider> CreateBatteryLevelProvider()
      override {
    // Start with a half-full battery
    battery_states_.push(MakeBatteryDischargingState(50));
    auto battery_provider =
        std::make_unique<FakeBatteryLevelProvider>(&battery_states_);
    return std::move(battery_provider);
  }

 protected:
  std::queue<std::optional<base::BatteryLevelProvider::BatteryState>>
      battery_states_;
};

class PowerMetricsReporterWithoutBatteryLevelProviderUnitTest
    : public PowerMetricsReporterUnitTestBase {
 public:
  ~PowerMetricsReporterWithoutBatteryLevelProviderUnitTest() override = default;

  std::unique_ptr<base::BatteryLevelProvider> CreateBatteryLevelProvider()
      override {
    return nullptr;
  }
};

}  // namespace

TEST_F(PowerMetricsReporterWithoutBatteryLevelProviderUnitTest,
       CPUTimeRecorded) {
  process_monitor_.SetMetricsToReturn(GetFakeProcessMetrics());

  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::Seconds(1);
  long_data_store_.SetIntervalDataToReturn(interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  const char* kScenarioSuffix = ".VideoCapture";
  const std::vector<const char*> suffixes({"", kScenarioSuffix});
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  // Windows ARM64 does not support Constant Rate TSC so
  // PerformanceMonitor.AverageCPU8.Total is not recorded there.
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.AverageCPU8.Total", std::nullopt}});
#else
  ExpectHistogramSamples(&histogram_tester_, suffixes,
                         {{"PerformanceMonitor.AverageCPU8.Total", 500}});
#endif
}

TEST_F(PowerMetricsReporterWithoutBatteryLevelProviderUnitTest,
       CPUTimeMissing) {
  process_monitor_.SetMetricsToReturn(
      GetFakeProcessMetrics(/*with_cpu_usage=*/false));

  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::Seconds(1);
  long_data_store_.SetIntervalDataToReturn(interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  const char* kScenarioSuffix = ".VideoCapture";
  const std::vector<const char*> suffixes({"", kScenarioSuffix});
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  // Windows ARM64 does not support Constant Rate TSC so
  // PerformanceMonitor.AverageCPU8.Total is not recorded there.
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.AverageCPU8.Total", std::nullopt}});
#else
  // Missing `cpu_usage` recorded as 0.
  ExpectHistogramSamples(&histogram_tester_, suffixes,
                         {{"PerformanceMonitor.AverageCPU8.Total", 0}});
#endif
}

TEST_F(PowerMetricsReporterUnitTest, LongIntervalHistograms) {
  process_monitor_.SetMetricsToReturn(GetFakeProcessMetrics());
  battery_states_.push(MakeBatteryDischargingState(30));

  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::Seconds(1);
  long_data_store_.SetIntervalDataToReturn(interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  const char* kScenarioSuffix = ".VideoCapture";
  const std::vector<const char*> suffixes({"", kScenarioSuffix});
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  // Windows ARM64 does not support Constant Rate TSC so
  // PerformanceMonitor.AverageCPU8.Total is not recorded there.
  ExpectHistogramSamples(
      &histogram_tester_, suffixes,
      {{"PerformanceMonitor.AverageCPU8.Total", std::nullopt}});
#else
  ExpectHistogramSamples(&histogram_tester_, suffixes,
                         {{"PerformanceMonitor.AverageCPU8.Total", 500}});
#endif
}

TEST_F(PowerMetricsReporterUnitTest, UKMs) {
  int fake_value = 42;

  ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = ++fake_value * 0.01;
#if BUILDFLAG(IS_MAC)
  fake_metrics.idle_wakeups = ++fake_value;
  fake_metrics.package_idle_wakeups = ++fake_value;
#endif
  process_monitor_.SetMetricsToReturn(fake_metrics);

  // Pretend that the battery has dropped by 20% in 2 minutes, for a rate of
  // 10% per minute.
  battery_states_.push(MakeBatteryDischargingState(30));

  UsageScenarioDataStore::IntervalData fake_interval_data;
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
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

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
      kLongPowerMetricsIntervalDuration.InSeconds() * 1000 *
          fake_metrics.cpu_usage.value());
#if BUILDFLAG(IS_MAC)
  test_ukm_recorder_.ExpectEntryMetric(entries[0], UkmEntry::kIdleWakeUpsName,
                                       fake_metrics.idle_wakeups);
  test_ukm_recorder_.ExpectEntryMetric(entries[0], UkmEntry::kPackageExitsName,
                                       fake_metrics.package_idle_wakeups);
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
      kLongPowerMetricsIntervalDuration.InSeconds());
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
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBrowserShuttingDown) {
  ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = 0.5;
#if BUILDFLAG(IS_MAC)
  fake_metrics.idle_wakeups = 42;
  fake_metrics.package_idle_wakeups = 43;
#endif
  process_monitor_.SetMetricsToReturn(fake_metrics);
  battery_states_.push(MakeBatteryDischargingState(50));

  const ukm::SourceId kTestSourceId =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  UsageScenarioDataStore::IntervalData fake_interval_data = {};
  fake_interval_data.source_id_for_longest_visible_origin = kTestSourceId;
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  {
    auto fake_shutdown = browser_shutdown::SetShutdownTypeForTesting(
        browser_shutdown::ShutdownType::kBrowserExit);
    EXPECT_TRUE(browser_shutdown::HasShutdownStarted());

    // Advance time while `HasShutdownStarted` has been overridden.
    task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);
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
  power_metrics_reporter_->battery_state_for_testing()
      ->is_external_power_connected = true;

  process_monitor_.SetMetricsToReturn({});

  // Push a battery state that indicates that the system is still not running
  // on battery.
  battery_states_.push(base::BatteryLevelProvider::BatteryState{
      .battery_count = 1,
      .is_external_power_connected = true,
      .current_capacity = 50,
      .full_charged_capacity = 100,
      .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      .capture_time = base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kPluggedIn));
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateChanges) {
  process_monitor_.SetMetricsToReturn({});

  // The initial battery state indicates that the system is running on battery,
  // pretends that this has changed.
  battery_states_.push(base::BatteryLevelProvider::BatteryState{
      .battery_count = 1,
      .is_external_power_connected = true,
      .current_capacity = 100,
      .full_charged_capacity = 100,
      .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh,
      .capture_time = base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kStateChanged));
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateUnavailable) {
  process_monitor_.SetMetricsToReturn({});

  // A nullopt battery value indicates that the battery level is unavailable.
  battery_states_.push(std::nullopt);

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kRetrievalError));
}

TEST_F(PowerMetricsReporterUnitTest, UKMsNoBattery) {
  power_metrics_reporter_->battery_state_for_testing()->battery_count = 0;
  power_metrics_reporter_->battery_state_for_testing()
      ->is_external_power_connected = true;

  process_monitor_.SetMetricsToReturn({});

  // Indicates that the system has no battery interface.
  battery_states_.push(base::BatteryLevelProvider::BatteryState{
      .battery_count = 0,
      .is_external_power_connected = true,
      .current_capacity = std::nullopt,
      .full_charged_capacity = std::nullopt,
      .charge_unit = std::nullopt,
      .capture_time = base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kNoBattery));
}

#if BUILDFLAG(IS_MAC)
// Tests that on MacOS, a full |charge_level| while not plugged does not result
// in a kDischarging value emitted. See https://crbug.com/1249830.
TEST_F(PowerMetricsReporterUnitTest, UKMsMacFullyCharged) {
  // Set the initial battery level at 100%.
  power_metrics_reporter_->battery_state_for_testing()->current_capacity = 100;

  process_monitor_.SetMetricsToReturn({});
  battery_states_.push(MakeBatteryDischargingState(100));

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kMacFullyCharged));
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateIncrease) {
  process_monitor_.SetMetricsToReturn({});

  // Set the new battery state at 75%.
  battery_states_.push(MakeBatteryDischargingState(75));

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  // An increase in charge level is reported as an invalid discharge rate.
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(BatteryDischargeMode::kBatteryLevelIncreased));
}

TEST_F(PowerMetricsReporterUnitTest, UKMsNoTab) {
  process_monitor_.SetMetricsToReturn(GetFakeProcessMetrics());
  battery_states_.push(MakeBatteryDischargingState(50));

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.max_tab_count = 0;
  fake_interval_data.max_visible_window_count = 0;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::kInvalidSourceId;
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

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
  process_monitor_.SetMetricsToReturn(GetFakeProcessMetrics());
  battery_states_.push(MakeBatteryDischargingState(50));

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.time_playing_video_full_screen_single_monitor =
      kLongPowerMetricsIntervalDuration * 100;
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(entries[0]->source_id, ukm::kInvalidSourceId);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kFullscreenVideoSingleMonitorSecondsName,
      // Every value greater than |kLongPowerMetricsIntervalDuration|
      // should fall in the same overflow bucket.
      PowerMetricsReporter::GetBucketForSampleForTesting(
          kLongPowerMetricsIntervalDuration * 2));
}

TEST_F(PowerMetricsReporterUnitTest, UKMsWithSleepEvent) {
  process_monitor_.SetMetricsToReturn({});
  battery_states_.push(MakeBatteryDischargingState(50));

  UsageScenarioDataStore::IntervalData fake_interval_data = {};
  fake_interval_data.sleep_events = 1;
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kDeviceSleptDuringIntervalName, true);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsWithoutCPU) {
  process_monitor_.SetMetricsToReturn(
      GetFakeProcessMetrics(/*with_cpu_usage=*/false));
  battery_states_.push(MakeBatteryDischargingState(30));

  UsageScenarioDataStore::IntervalData fake_interval_data = {};
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  long_data_store_.SetIntervalDataToReturn(fake_interval_data);

  task_environment_.FastForwardBy(kLongPowerMetricsIntervalDuration);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  ASSERT_EQ(1u, entries.size());

  EXPECT_EQ(entries[0]->source_id,
            fake_interval_data.source_id_for_longest_visible_origin);
  // Missing `cpu_usage` should be skipped, not logged as 0% CPU.
  EXPECT_FALSE(
      test_ukm_recorder_.EntryHasMetric(entries[0], UkmEntry::kCPUTimeMsName));
}
