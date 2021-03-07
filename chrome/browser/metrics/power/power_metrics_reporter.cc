// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

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

}  // namespace

PowerMetricsReporter::PowerMetricsReporter(
    const base::WeakPtr<UsageScenarioDataStore>& data_store,
    std::unique_ptr<BatteryLevelProvider> battery_level_provider)
    : data_store_(data_store),
      battery_level_provider_(std::move(battery_level_provider)) {
  DCHECK(performance_monitor::ProcessMonitor::Get());
  performance_monitor::ProcessMonitor::Get()->AddObserver(this);
  battery_state_ = battery_level_provider_->GetBatteryState();
}

PowerMetricsReporter::~PowerMetricsReporter() {
  if (auto* process_monitor = performance_monitor::ProcessMonitor::Get()) {
    process_monitor->RemoveObserver(this);
  }
}

void PowerMetricsReporter::OnAggregatedMetricsSampled(
    const performance_monitor::ProcessMonitor::Metrics& metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* process_monitor = performance_monitor::ProcessMonitor::Get();
  base::TimeDelta sampling_interval =
      process_monitor->GetScheduledSamplingInterval();

  auto now = base::TimeTicks::Now();
  base::TimeDelta interval_duration = now - interval_begin_;
  interval_begin_ = now;

  auto discharge_mode_and_rate =
      GetBatteryDischargeRateDuringInterval(interval_duration);

  ReportHistograms(sampling_interval, interval_duration,
                   discharge_mode_and_rate.first,
                   discharge_mode_and_rate.second);
  if (report_ukms_) {
    ReportUKMs(metrics, interval_duration, discharge_mode_and_rate.first,
               discharge_mode_and_rate.second);
  }
}

void PowerMetricsReporter::ReportHistograms(
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

  // TODO(sebmarchand): Figure out if we need to report data when we don't have
  // a valid sourceID.
  if (source_id == ukm::kInvalidSourceId)
    return;

  ukm::builders::PowerUsageScenariosIntervalData builder(source_id);
  builder.SetURLVisibilityTimeSeconds(ukm::GetExponentialBucketMinForUserTiming(
      usage_metrics.source_id_for_longest_visible_origin_duration.InSeconds()));
  builder.SetIntervalDurationSeconds(interval_duration.InSeconds());
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
      ukm::GetExponentialBucketMinForUserTiming(
          usage_metrics.time_playing_video_in_visible_tab.InSeconds()));
  builder.SetTopLevelNavigationEvents(ukm::GetExponentialBucketMinForCounts1000(
      usage_metrics.top_level_navigation_count));
  builder.SetUserInteractionCount(ukm::GetExponentialBucketMinForCounts1000(
      usage_metrics.user_interaction_count));
  builder.SetFullscreenVideoSingleMonitorSeconds(
      ukm::GetExponentialBucketMinForUserTiming(
          usage_metrics.time_playing_video_full_screen_single_monitor
              .InSeconds()));
  builder.SetTimeWithOpenWebRTCConnectionSeconds(
      ukm::GetExponentialBucketMinForUserTiming(
          usage_metrics.time_with_open_webrtc_connection.InSeconds()));
  builder.SetTimeSinceInteractionWithBrowserSeconds(
      ukm::GetExponentialBucketMinForUserTiming(
          usage_metrics.time_since_last_user_interaction_with_browser
              .InSeconds()));
  builder.SetVideoCaptureSeconds(ukm::GetExponentialBucketMinForUserTiming(
      usage_metrics.time_capturing_video.InSeconds()));
  builder.SetBrowserShuttingDown(browser_shutdown::HasShutdownStarted());

  builder.Record(ukm_recorder);
}

std::pair<PowerMetricsReporter::BatteryDischargeMode, base::Optional<int64_t>>
PowerMetricsReporter::GetBatteryDischargeRateDuringInterval(
    base::TimeDelta interval_duration) {
  auto previous_battery_state =
      std::exchange(battery_state_, battery_level_provider_->GetBatteryState());

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
