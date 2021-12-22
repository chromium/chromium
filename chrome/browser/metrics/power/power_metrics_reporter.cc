// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/metrics/power/power_details_provider.h"
#include "chrome/browser/performance_monitor/process_metrics_recorder_util.h"
#include "chrome/browser/performance_monitor/process_monitor.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace {

constexpr const char* kBatteryDischargeRateHistogramName =
    "Power.BatteryDischargeRate2";
constexpr const char* kBatteryDischargeModeHistogramName =
    "Power.BatteryDischargeMode";
constexpr const char* kBatterySamplingDelayHistogramName =
    "Power.BatterySamplingDelay";
constexpr const char* kMainScreenBrightnessHistogramName =
    "Power.MainScreenBrightness2";
constexpr const char* kMainScreenBrightnessAvailableHistogramName =
    "Power.MainScreenBrightnessAvailable";

// Calculates the UKM bucket |value| falls in and returns it. This uses an
// exponential bucketing approach with an exponent base of 1.3, resulting in
// 17 buckets for an interval of 120 seconds.
int64_t GetBucketForSample(base::TimeDelta value) {
  const float kBucketSpacing = 1.3;
  // Put all the abnormal values in an overflow bucket. The default interval
  // length is 120 seconds, with an exponent base of 1.3 the bucket for this
  // value includes all values in the [113, 146] range.
  constexpr int64_t kOverflowBucket = 147;
  DCHECK_EQ(kOverflowBucket,
            ukm::GetExponentialBucketMin(kOverflowBucket, kBucketSpacing));
  return std::min(
      ukm::GetExponentialBucketMin(value.InSeconds(), kBucketSpacing),
      kOverflowBucket);
}

// Returns the scenario-specific suffix to use for metrics captured during an
// interval described by |interval_data|.
const char* GetScenarioSuffix(
    const UsageScenarioDataStore::IntervalData& interval_data) {
  // Important: The order of the conditions is important. See the full
  // description of each scenario in the histograms.xml file.
  if (interval_data.max_tab_count == 0)
    return ".ZeroWindow";
  if (interval_data.max_visible_window_count == 0)
    return ".AllTabsHidden";
  if (!interval_data.time_capturing_video.is_zero())
    return ".VideoCapture";
  if (!interval_data.time_playing_video_full_screen_single_monitor.is_zero())
    return ".FullscreenVideo";
  if (!interval_data.time_playing_video_in_visible_tab.is_zero()) {
    // Note: UKM data reveals that navigations are infrequent when a video is
    // playing in fullscreen, when video is captured or when audio is playing.
    // For that reason, there is no distinct suffix for navigation vs. no
    // navigation in these cases.
    if (interval_data.top_level_navigation_count == 0)
      return ".EmbeddedVideo_NoNavigation";
    return ".EmbeddedVideo_WithNavigation";
  }
  if (!interval_data.time_playing_audio.is_zero())
    return ".Audio";
  if (interval_data.top_level_navigation_count > 0)
    return ".Navigation";
  if (interval_data.user_interaction_count > 0)
    return ".Interaction";
  return ".Passive";
}

// Returns the histogram suffixes to use for metrics captured during an interval
// described by |interval_data|.
std::vector<const char*> GetSuffixes(
    const UsageScenarioDataStore::IntervalData& interval_data) {
  // Histograms are recorded without suffix and with a scenario-specific suffix.
  return std::vector<const char*>{"", GetScenarioSuffix(interval_data)};
}

}  // namespace

PowerMetricsReporter::PowerMetricsReporter(
    const base::WeakPtr<UsageScenarioDataStore>& data_store,
    std::unique_ptr<BatteryLevelProvider> battery_level_provider)
    : data_store_(data_store),
      battery_level_provider_(std::move(battery_level_provider)) {
  DCHECK(performance_monitor::ProcessMonitor::Get());
  performance_monitor::ProcessMonitor::Get()->AddObserver(this);

  battery_level_provider_->GetBatteryState(
      base::BindOnce(&PowerMetricsReporter::OnFirstBatteryStateSampled,
                     weak_factory_.GetWeakPtr()));

#if defined(OS_MAC)
  power_details_provider_ = PowerDetailsProvider::Create();
  iopm_power_source_sampling_event_source_.Start(
      base::BindRepeating(&PowerMetricsReporter::OnIOPMPowerSourceSamplingEvent,
                          base::Unretained(this)));
#endif
}

PowerMetricsReporter::~PowerMetricsReporter() {
  if (auto* process_monitor = performance_monitor::ProcessMonitor::Get()) {
    process_monitor->RemoveObserver(this);
  }
}

void PowerMetricsReporter::OnFirstSampleForTesting(base::OnceClosure closure) {
  if (!interval_begin_.is_null()) {
    std::move(closure).Run();
  } else {
    on_battery_sampled_for_testing_ = std::move(closure);
  }
}

int64_t PowerMetricsReporter::GetBucketForSampleForTesting(
    base::TimeDelta value) {
  return GetBucketForSample(value);
}

void PowerMetricsReporter::OnAggregatedMetricsSampled(
    const performance_monitor::ProcessMonitor::Metrics& metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  battery_level_provider_->GetBatteryState(
      base::BindOnce(&PowerMetricsReporter::OnBatteryStateAndMetricsSampled,
                     weak_factory_.GetWeakPtr(), metrics,
                     /* scheduled_time=*/base::TimeTicks::Now()));
}

std::vector<const char*> PowerMetricsReporter::GetSuffixesForTesting(
    const UsageScenarioDataStore::IntervalData& interval_data) {
  return GetSuffixes(interval_data);
}

void PowerMetricsReporter::ReportHistograms(
    const UsageScenarioDataStore::IntervalData& interval_data,
    const performance_monitor::ProcessMonitor::Metrics& metrics,
    base::TimeDelta interval_duration,
    BatteryDischargeMode discharge_mode,
    absl::optional<int64_t> discharge_rate_during_interval) {
  const std::vector<const char*> suffixes = GetSuffixes(interval_data);
  ReportCPUHistograms(metrics, suffixes);
  ReportBatteryHistograms(interval_duration, discharge_mode,
                          discharge_rate_during_interval, suffixes);
#if defined(OS_MAC)
  RecordCoalitionData(metrics, suffixes);
#endif
}

void PowerMetricsReporter::ReportBatteryHistograms(
    base::TimeDelta interval_duration,
    BatteryDischargeMode discharge_mode,
    absl::optional<int64_t> discharge_rate_during_interval,
    const std::vector<const char*>& suffixes) {
  // Ratio by which the time elapsed can deviate from
  // |performance_monitor::ProcessMonitor::kGatherInterval| without invalidating
  // this sample.
  constexpr double kTolerableTimeElapsedRatio = 0.10;
  constexpr double kTolerablePositiveDrift = (1. + kTolerableTimeElapsedRatio);
  constexpr double kTolerableNegativeDrift = (1. - kTolerableTimeElapsedRatio);

  if (discharge_mode == BatteryDischargeMode::kDischarging &&
      interval_duration > performance_monitor::ProcessMonitor::kGatherInterval *
                              kTolerablePositiveDrift) {
    // Too much time passed since the last record. Either the task took
    // too long to get executed or system sleep took place.
    discharge_mode = BatteryDischargeMode::kInvalidInterval;
  }

  if (discharge_mode == BatteryDischargeMode::kDischarging &&
      interval_duration < performance_monitor::ProcessMonitor::kGatherInterval *
                              kTolerableNegativeDrift) {
    // The recording task executed too early after the previous one, possibly
    // because the previous task took too long to execute.
    discharge_mode = BatteryDischargeMode::kInvalidInterval;
  }

  for (const char* suffix : suffixes) {
    base::UmaHistogramEnumeration(
        base::JoinString({kBatteryDischargeModeHistogramName, suffix}, ""),
        discharge_mode);

    if (discharge_mode == BatteryDischargeMode::kDischarging) {
      DCHECK(discharge_rate_during_interval.has_value());
      base::UmaHistogramCounts1000(
          base::JoinString({kBatteryDischargeRateHistogramName, suffix}, ""),
          *discharge_rate_during_interval);
    }
  }
}

void PowerMetricsReporter::OnFirstBatteryStateSampled(
    const BatteryLevelProvider::BatteryState& battery_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  battery_state_ = battery_state;
  interval_begin_ = base::TimeTicks::Now();
  if (on_battery_sampled_for_testing_)
    std::move(on_battery_sampled_for_testing_).Run();
}

void PowerMetricsReporter::OnBatteryStateAndMetricsSampled(
    const performance_monitor::ProcessMonitor::Metrics& metrics,
    base::TimeTicks scheduled_time,
    const BatteryLevelProvider::BatteryState& battery_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto now = base::TimeTicks::Now();
  base::TimeDelta interval_duration = now - interval_begin_;
  interval_begin_ = now;
  base::UmaHistogramMicrosecondsTimes(kBatterySamplingDelayHistogramName,
                                      now - scheduled_time);

  auto discharge_mode_and_rate =
      GetBatteryDischargeRateDuringInterval(battery_state, interval_duration);
  ReportUKMsAndHistograms(metrics, interval_duration,
                          discharge_mode_and_rate.first,
                          discharge_mode_and_rate.second);

  if (on_battery_sampled_for_testing_)
    std::move(on_battery_sampled_for_testing_).Run();
}

void PowerMetricsReporter::ReportUKMsAndHistograms(
    const performance_monitor::ProcessMonitor::Metrics& metrics,
    base::TimeDelta interval_duration,
    BatteryDischargeMode discharge_mode,
    absl::optional<int64_t> discharge_rate_during_interval) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_store_.MaybeValid());

  UsageScenarioDataStore::IntervalData interval_data =
      data_store_->ResetIntervalData();

  absl::optional<int64_t> main_screen_brightness;
  if (power_details_provider_.get()) {
    absl::optional<double> brightness =
        power_details_provider_->GetMainScreenBrightnessLevel();
    if (brightness.has_value()) {
      // Report the percentage as an integer as UMA doesn't allow reporting
      // reals.
      main_screen_brightness = brightness.value() * 100;
      // The brightness value reported by the system sometimes exceeds 100%,
      // allow values up to 150 to understand this better.
      // An histogram with 50 buckets, a minimum of 1 and a maximum of 150 will
      // have 43 buckets in the [1, 100] range and 7 in the 100+ range.
      base::UmaHistogramCustomCounts(kMainScreenBrightnessHistogramName,
                                     main_screen_brightness.value(), 1, 150,
                                     50);
    }
  }
  base::UmaHistogramBoolean(kMainScreenBrightnessAvailableHistogramName,
                            main_screen_brightness.has_value());

  ReportUKMs(interval_data, metrics, interval_duration, discharge_mode,
             discharge_rate_during_interval, main_screen_brightness);

  ReportHistograms(interval_data, metrics, interval_duration, discharge_mode,
                   discharge_rate_during_interval);
}

// static
void PowerMetricsReporter::ReportCPUHistograms(
    const performance_monitor::ProcessMonitor::Metrics& metrics,
    const std::vector<const char*>& suffixes) {
  for (const char* suffix : suffixes) {
    std::string complete_suffix = base::StrCat({"Total", suffix});
    performance_monitor::RecordProcessHistograms(complete_suffix.c_str(),
                                                 metrics);
  }
}

void PowerMetricsReporter::ReportUKMs(
    const UsageScenarioDataStore::IntervalData& interval_data,
    const performance_monitor::ProcessMonitor::Metrics& metrics,
    base::TimeDelta interval_duration,
    BatteryDischargeMode discharge_mode,
    absl::optional<int64_t> discharge_rate_during_interval,
    absl::optional<int64_t> main_screen_brightness) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_store_.MaybeValid());

  // UKM may be unavailable in content_shell or other non-chrome/ builds; it
  // may also be unavailable if browser shutdown has started; so this may be a
  // nullptr. If it's unavailable, UKM reporting will be skipped.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  auto source_id = interval_data.source_id_for_longest_visible_origin;

  // Only navigation SourceIds should be associated with this UKM.
  if (source_id != ukm::kInvalidSourceId) {
    // TODO(crbug.com/1153193): Change to a DCHECK in August 2021, after we've
    // validated that the condition is always met in production.
    CHECK_EQ(ukm::GetSourceIdType(source_id), ukm::SourceIdType::NAVIGATION_ID);
  }

  ukm::builders::PowerUsageScenariosIntervalData builder(source_id);

  builder.SetURLVisibilityTimeSeconds(GetBucketForSample(
      interval_data.source_id_for_longest_visible_origin_duration));
  builder.SetIntervalDurationSeconds(interval_duration.InSeconds());
  // An exponential bucket is fine here as this value isn't limited to the
  // interval duration.
  builder.SetUptimeSeconds(ukm::GetExponentialBucketMinForUserTiming(
      interval_data.uptime_at_interval_end.InSeconds()));
  builder.SetBatteryDischargeMode(static_cast<int64_t>(discharge_mode));
  if (discharge_mode == BatteryDischargeMode::kDischarging) {
    DCHECK(discharge_rate_during_interval.has_value());
    builder.SetBatteryDischargeRate(*discharge_rate_during_interval);
  }
  builder.SetCPUTimeMs(metrics.cpu_usage * interval_duration.InMilliseconds());
#if defined(OS_MAC)
  builder.SetIdleWakeUps(metrics.idle_wakeups);
  builder.SetPackageExits(metrics.package_idle_wakeups);
  builder.SetEnergyImpactScore(metrics.energy_impact);
#endif
  builder.SetMaxTabCount(
      ukm::GetExponentialBucketMinForCounts1000(interval_data.max_tab_count));
  // The number of windows is usually relatively low, use a small bucket
  // spacing.
  builder.SetMaxVisibleWindowCount(ukm::GetExponentialBucketMin(
      interval_data.max_visible_window_count, 1.05));
  builder.SetTabClosed(ukm::GetExponentialBucketMinForCounts1000(
      interval_data.tabs_closed_during_interval));
  builder.SetTimePlayingVideoInVisibleTab(
      GetBucketForSample(interval_data.time_playing_video_in_visible_tab));
  builder.SetTopLevelNavigationEvents(ukm::GetExponentialBucketMinForCounts1000(
      interval_data.top_level_navigation_count));
  builder.SetUserInteractionCount(ukm::GetExponentialBucketMinForCounts1000(
      interval_data.user_interaction_count));
  builder.SetFullscreenVideoSingleMonitorSeconds(GetBucketForSample(
      interval_data.time_playing_video_full_screen_single_monitor));
  builder.SetTimeWithOpenWebRTCConnectionSeconds(
      GetBucketForSample(interval_data.time_with_open_webrtc_connection));
  builder.SetTimeSinceInteractionWithBrowserSeconds(GetBucketForSample(
      interval_data.time_since_last_user_interaction_with_browser));
  builder.SetVideoCaptureSeconds(
      GetBucketForSample(interval_data.time_capturing_video));
  builder.SetBrowserShuttingDown(browser_shutdown::HasShutdownStarted());
  builder.SetPlayingAudioSeconds(
      GetBucketForSample(interval_data.time_playing_audio));
  builder.SetOriginVisibilityTimeSeconds(
      GetBucketForSample(interval_data.longest_visible_origin_duration));
  if (main_screen_brightness.has_value()) {
    // The data should be reported with a 20% granularity.
    builder.SetMainScreenBrightnessPercent(
        ukm::GetLinearBucketMin(main_screen_brightness.value(), 20));
  }
  builder.SetDeviceSleptDuringInterval(interval_data.sleep_events);

  builder.Record(ukm_recorder);
}

std::pair<PowerMetricsReporter::BatteryDischargeMode, absl::optional<int64_t>>
PowerMetricsReporter::GetBatteryDischargeRateDuringInterval(
    const BatteryLevelProvider::BatteryState& new_battery_state,
    base::TimeDelta interval_duration) {
  auto previous_battery_state =
      std::exchange(battery_state_, new_battery_state);

  if (previous_battery_state.battery_count == 0 ||
      battery_state_.battery_count == 0) {
    return {BatteryDischargeMode::kNoBattery, absl::nullopt};
  }
  if (!previous_battery_state.on_battery && !battery_state_.on_battery) {
    return {BatteryDischargeMode::kPluggedIn, absl::nullopt};
  }
  if (previous_battery_state.on_battery != battery_state_.on_battery) {
    return {BatteryDischargeMode::kStateChanged, absl::nullopt};
  }
  if (!previous_battery_state.charge_level.has_value() ||
      !battery_state_.charge_level.has_value()) {
    return {BatteryDischargeMode::kChargeLevelUnavailable, absl::nullopt};
  }

  // The battery discharge rate is reported per minute with 1/10000 of full
  // charge resolution.
  static const int64_t kDischargeRateFactor =
      10000 * base::Minutes(1).InSecondsF();

#if defined(OS_MAC)
  // On MacOS, empirical evidence has shown that right after a full charge, the
  // current capacity stays equal to the maximum capacity for several minutes,
  // despite the fact that power was definitely consumed. Reporting a zero
  // discharge rate for this duration would be misleading.
  if (previous_battery_state.charge_level.value() == 1.0)
    return {BatteryDischargeMode::kMacFullyCharged, absl::nullopt};
#endif

  auto discharge_rate = (previous_battery_state.charge_level.value() -
                         battery_state_.charge_level.value()) *
                        kDischargeRateFactor / interval_duration.InSeconds();
  if (discharge_rate < 0)
    return {BatteryDischargeMode::kBatteryLevelIncreased, absl::nullopt};
  return {BatteryDischargeMode::kDischarging, discharge_rate};
}

#if defined(OS_MAC)
void PowerMetricsReporter::OnIOPMPowerSourceSamplingEvent() {
  base::TimeTicks now_ticks = base::TimeTicks::Now();

  if (!last_event_time_ticks_) {
    last_event_time_ticks_ = now_ticks;
    return;
  }

  // The delta is expected to be almost always 60 seconds. Split the buckets for
  // 0.2s granularity (10s interval with 50 buckets + 1 underflow bucket + 1
  // overflow bucket) around that value.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryTimeGet(
      "Power.IOPMPowerSource.SamplingEventDelta",
      /*min=*/base::Seconds(55), /*max=*/base::Seconds(65), /*buckets=*/52,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(now_ticks - *last_event_time_ticks_);
  *last_event_time_ticks_ = now_ticks;
}
#endif  // defined(OS_MAC)
