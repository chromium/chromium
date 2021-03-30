// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
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

void PowerMetricsReporter::ReportBatteryHistograms(
    base::TimeDelta sampling_interval,
    base::TimeDelta interval_duration,
    BatteryDischargeMode discharge_mode,
    base::Optional<int64_t> discharge_rate_during_interval) {
  // Ratio by which the time elapsed can deviate from |recording_interval|
  // without invalidating this sample.
  constexpr double kTolerableTimeElapsedRatio = 0.10;
  constexpr double kTolerablePositiveDrift = (1. + kTolerableTimeElapsedRatio);
  constexpr double kTolerableNegativeDrift = (1. - kTolerableTimeElapsedRatio);

  if (discharge_mode == BatteryDischargeMode::kDischarging &&
      interval_duration > sampling_interval * kTolerablePositiveDrift) {
    // Too much time passed since the last record. Either the task took
    // too long to get executed or system sleep took place.
    discharge_mode = BatteryDischargeMode::kInvalidInterval;
  }

  if (discharge_mode == BatteryDischargeMode::kDischarging &&
      interval_duration < sampling_interval * kTolerableNegativeDrift) {
    // The recording task executed too early after the previous one, possibly
    // because the previous task took too long to execute.
    discharge_mode = BatteryDischargeMode::kInvalidInterval;
  }

  base::UmaHistogramEnumeration(kBatteryDischargeModeHistogramName,
                                discharge_mode);
  if (discharge_mode == BatteryDischargeMode::kDischarging) {
    DCHECK(discharge_rate_during_interval.has_value());
    base::UmaHistogramCounts1000(kBatteryDischargeRateHistogramName,
                                 *discharge_rate_during_interval);
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

  auto* process_monitor = performance_monitor::ProcessMonitor::Get();
  base::TimeDelta sampling_interval =
      process_monitor->GetScheduledSamplingInterval();

  auto now = base::TimeTicks::Now();
  base::TimeDelta interval_duration = now - interval_begin_;
  interval_begin_ = now;

  base::UmaHistogramMicrosecondsTimes(kBatterySamplingDelayHistogramName,
                                      now - scheduled_time);

  auto discharge_mode_and_rate =
      GetBatteryDischargeRateDuringInterval(battery_state, interval_duration);

  ReportBatteryHistograms(sampling_interval, interval_duration,
                          discharge_mode_and_rate.first,
                          discharge_mode_and_rate.second);

  ReportUKMs(metrics, interval_duration, discharge_mode_and_rate.first,
             discharge_mode_and_rate.second);

  if (on_battery_sampled_for_testing_)
    std::move(on_battery_sampled_for_testing_).Run();
}

void PowerMetricsReporter::ReportUKMs(
    const performance_monitor::ProcessMonitor::Metrics& metrics,
    base::TimeDelta interval_duration,
    BatteryDischargeMode discharge_mode,
    base::Optional<int64_t> discharge_rate_during_interval) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_store_.MaybeValid());

  // UKM may be unavailable in content_shell or other non-chrome/ builds; it
  // may also be unavailable if browser shutdown has started; so this may be a
  // nullptr. If it's unavailable, UKM reporting will be skipped.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  auto usage_metrics = data_store_->ResetIntervalData();
  auto source_id = usage_metrics.source_id_for_longest_visible_origin;

  ukm::builders::PowerUsageScenariosIntervalData builder(source_id);

  builder.SetURLVisibilityTimeSeconds(GetBucketForSample(
      usage_metrics.source_id_for_longest_visible_origin_duration));
  builder.SetIntervalDurationSeconds(interval_duration.InSeconds());
  // An exponential bucket is fine here as this value isn't limited to the
  // interval duration.
  builder.SetUptimeSeconds(ukm::GetExponentialBucketMinForUserTiming(
      usage_metrics.uptime_at_interval_end.InSeconds()));
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
      ukm::GetExponentialBucketMinForCounts1000(usage_metrics.max_tab_count));
  // The number of windows is usually relatively low, use a small bucket
  // spacing.
  builder.SetMaxVisibleWindowCount(ukm::GetExponentialBucketMin(
      usage_metrics.max_visible_window_count, 1.05));
  builder.SetTabClosed(ukm::GetExponentialBucketMinForCounts1000(
      usage_metrics.tabs_closed_during_interval));
  builder.SetTimePlayingVideoInVisibleTab(
      GetBucketForSample(usage_metrics.time_playing_video_in_visible_tab));
  builder.SetTopLevelNavigationEvents(ukm::GetExponentialBucketMinForCounts1000(
      usage_metrics.top_level_navigation_count));
  builder.SetUserInteractionCount(ukm::GetExponentialBucketMinForCounts1000(
      usage_metrics.user_interaction_count));
  builder.SetFullscreenVideoSingleMonitorSeconds(GetBucketForSample(
      usage_metrics.time_playing_video_full_screen_single_monitor));
  builder.SetTimeWithOpenWebRTCConnectionSeconds(
      GetBucketForSample(usage_metrics.time_with_open_webrtc_connection));
  builder.SetTimeSinceInteractionWithBrowserSeconds(GetBucketForSample(
      usage_metrics.time_since_last_user_interaction_with_browser));
  builder.SetVideoCaptureSeconds(
      GetBucketForSample(usage_metrics.time_capturing_video));
  builder.SetBrowserShuttingDown(browser_shutdown::HasShutdownStarted());
  builder.SetPlayingAudioSeconds(
      GetBucketForSample(usage_metrics.time_playing_audio));
  builder.SetOriginVisibilityTimeSeconds(
      GetBucketForSample(usage_metrics.longest_visible_origin_duration));

  builder.Record(ukm_recorder);
}

std::pair<PowerMetricsReporter::BatteryDischargeMode, base::Optional<int64_t>>
PowerMetricsReporter::GetBatteryDischargeRateDuringInterval(
    const BatteryLevelProvider::BatteryState& new_battery_state,
    base::TimeDelta interval_duration) {
  auto previous_battery_state =
      std::exchange(battery_state_, new_battery_state);

  if (previous_battery_state.battery_count == 0 ||
      battery_state_.battery_count == 0) {
    return {BatteryDischargeMode::kNoBattery, base::nullopt};
  }
  if (!previous_battery_state.on_battery && !battery_state_.on_battery) {
    return {BatteryDischargeMode::kPluggedIn, base::nullopt};
  }
  if (previous_battery_state.on_battery != battery_state_.on_battery) {
    return {BatteryDischargeMode::kStateChanged, base::nullopt};
  }
  if (!previous_battery_state.charge_level.has_value() ||
      !battery_state_.charge_level.has_value()) {
    return {BatteryDischargeMode::kChargeLevelUnavailable, base::nullopt};
  }

  // The battery discharge rate is reported per minute with 1/10000 of full
  // charge resolution.
  static const int64_t kDischargeRateFactor =
      10000 * base::TimeDelta::FromMinutes(1).InSecondsF();

  auto discharge_rate = (previous_battery_state.charge_level.value() -
                         battery_state_.charge_level.value()) *
                        kDischargeRateFactor / interval_duration.InSeconds();
  if (discharge_rate < 0)
    return {BatteryDischargeMode::kInvalidDischargeRate, base::nullopt};
  return {BatteryDischargeMode::kDischarging, discharge_rate};
}
