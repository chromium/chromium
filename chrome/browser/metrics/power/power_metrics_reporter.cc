// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/metrics/power/power_metrics.h"
#include "chrome/browser/metrics/power/power_metrics_constants.h"
#include "chrome/browser/metrics/power/process_metrics_recorder_util.h"
#include "chrome/browser/metrics/power/process_monitor.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace {

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

// Returns the histogram suffix to be used given the MonitoredProcessType.
const char* GetMetricSuffixFromProcessType(MonitoredProcessType type) {
  switch (type) {
    case MonitoredProcessType::kBrowser:
      return "BrowserProcess";
    case MonitoredProcessType::kRenderer:
      return "RendererProcess";
    case MonitoredProcessType::kExtensionPersistent:
      return "RendererExtensionPersistentProcess";
    case MonitoredProcessType::kExtensionEvent:
      return "RendererExtensionEventProcess";
    case MonitoredProcessType::kGpu:
      return "GPUProcess";
    case MonitoredProcessType::kUtility:
      return "UtilityProcess";
    case MonitoredProcessType::kNetwork:
      return "NetworkProcess";
    case MonitoredProcessType::kOther:
      return "OtherProcess";
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

}  // namespace

PowerMetricsReporter::PowerMetricsReporter(
    ProcessMonitor* process_monitor,
    UsageScenarioDataStore* long_usage_scenario_data_store,
    std::unique_ptr<base::BatteryLevelProvider> battery_level_provider)
    : process_monitor_(process_monitor),
      long_usage_scenario_data_store_(long_usage_scenario_data_store),
      battery_level_provider_(std::move(battery_level_provider)) {
  if (!long_usage_scenario_data_store_) {
    long_usage_scenario_tracker_ = std::make_unique<UsageScenarioTracker>();
    long_usage_scenario_data_store_ =
        long_usage_scenario_tracker_->data_store();
  }

  interval_begin_ = base::TimeTicks::Now();

  // `battery_level_provider_` may be null on platforms that do not have an
  // implementation.
  if (battery_level_provider_) {
    // Unretained() is safe here because |this| outlive
    // |battery_level_provider_|.
    battery_level_provider_->GetBatteryState(
        base::BindOnce(&PowerMetricsReporter::OnFirstBatteryStateSampled,
                       base::Unretained(this)));
  }

  StartNextLongInterval();
}

PowerMetricsReporter::~PowerMetricsReporter() = default;

// static
int64_t PowerMetricsReporter::GetBucketForSampleForTesting(
    base::TimeDelta value) {
  return GetBucketForSample(value);
}

void PowerMetricsReporter::OnFirstBatteryStateSampled(
    const std::optional<base::BatteryLevelProvider::BatteryState>&
        battery_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(battery_level_provider_);
  battery_state_ = battery_state;
}

void PowerMetricsReporter::StartNextLongInterval() {
  // TODO(fdoray): Remove when no longer referenced by server-side trace
  // configs, planned for 06/2024.
  base::trace_event::EmitNamedTrigger("power-metrics-interval-start");

  interval_timer_.Start(FROM_HERE, kLongPowerMetricsIntervalDuration,
                        base::BindOnce(&PowerMetricsReporter::OnLongIntervalEnd,
                                       base::Unretained(this)));
}

void PowerMetricsReporter::OnLongIntervalEnd() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Sample the metrics for all processes. This will call back into
  // OnAggregatedMetricsSampled() once done (synchronously).
  process_monitor_->SampleAllProcesses(this);
}

void PowerMetricsReporter::OnMetricsSampled(
    MonitoredProcessType type,
    const ProcessMonitor::Metrics& metrics) {
  RecordProcessHistograms(GetMetricSuffixFromProcessType(type), metrics);
}

void PowerMetricsReporter::OnAggregatedMetricsSampled(
    const ProcessMonitor::Metrics& metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Evaluate the interval duration.
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta interval_duration = now - interval_begin_;
  interval_begin_ = now;

  // Finally, retrieve the battery state before reporting the metrics. On
  // platform without a BatteryLevelProvider implementation, skip straight to
  // reporting the metrics.
  if (battery_level_provider_) {
    // Note: The use of `Unretained()` is safe here because |this| outlives
    //       |battery_level_provider_|.
    battery_level_provider_->GetBatteryState(base::BindOnce(
        &PowerMetricsReporter::OnBatteryAndAggregatedProcessMetricsSampled,
        base::Unretained(this), metrics, interval_duration));
  } else {
    // Get usage scenario data.
    auto long_interval_data =
        long_usage_scenario_data_store_->ResetIntervalData();
    ReportMetrics(long_interval_data, interval_duration, metrics);
  }
}

void PowerMetricsReporter::OnBatteryAndAggregatedProcessMetricsSampled(
    const ProcessMonitor::Metrics& aggregated_process_metrics,
    base::TimeDelta interval_duration,
    const std::optional<base::BatteryLevelProvider::BatteryState>&
        new_battery_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(battery_level_provider_);

  // Evaluate battery discharge mode and rate.
  auto previous_battery_state =
      std::exchange(battery_state_, new_battery_state);
  auto battery_discharge = GetBatteryDischargeDuringInterval(
      previous_battery_state, new_battery_state, interval_duration);

  // Get usage scenario data.
  auto long_interval_data =
      long_usage_scenario_data_store_->ResetIntervalData();
  ReportMetrics(long_interval_data, interval_duration,
                aggregated_process_metrics);
  ReportBatterySpecificMetrics(long_interval_data, interval_duration,
                               aggregated_process_metrics, battery_discharge);
}

void PowerMetricsReporter::ReportMetrics(
    const UsageScenarioDataStore::IntervalData& long_interval_data,
    base::TimeDelta interval_duration,
    const ProcessMonitor::Metrics& aggregated_process_metrics) {
  // Get scenario data.
  const auto long_interval_scenario_params =
      GetLongIntervalScenario(long_interval_data);
  // Histograms are recorded without suffix and with a scenario-specific
  // suffix.
  const std::vector<const char*> long_interval_suffixes{
      "", long_interval_scenario_params.histogram_suffix};

  // Report process metrics histograms.
  ReportAggregatedProcessMetricsHistograms(aggregated_process_metrics,
                                           long_interval_suffixes);
  base::UmaHistogramEnumeration("PerformanceMonitor.UsageScenario.LongInterval",
                                long_interval_scenario_params.scenario);

  StartNextLongInterval();
}

void PowerMetricsReporter::ReportBatterySpecificMetrics(
    const UsageScenarioDataStore::IntervalData& long_interval_data,
    base::TimeDelta interval_duration,
    const ProcessMonitor::Metrics& aggregated_process_metrics,
    BatteryDischarge battery_discharge) {
  DCHECK(battery_level_provider_);

  // Report UKMs.
  ReportBatteryUKMs(long_interval_data, aggregated_process_metrics,
                    interval_duration, battery_discharge);
}

void PowerMetricsReporter::ReportBatteryUKMs(
    const UsageScenarioDataStore::IntervalData& interval_data,
    const ProcessMonitor::Metrics& metrics,
    base::TimeDelta interval_duration,
    BatteryDischarge battery_discharge) {
  // UKM may be unavailable in content_shell or other non-chrome/ builds; it
  // may also be unavailable if browser shutdown has started; so this may be a
  // nullptr. If it's unavailable, UKM reporting will be skipped.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  auto source_id = interval_data.source_id_for_longest_visible_origin;

  // Only navigation SourceIds should be associated with this UKM.
  if (source_id != ukm::kInvalidSourceId) {
    // TODO(crbug.com/40158987): Change to a DCHECK in August 2021, after we've
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
  builder.SetBatteryDischargeMode(static_cast<int64_t>(battery_discharge.mode));
  if (battery_discharge.mode == BatteryDischargeMode::kDischarging) {
    DCHECK(battery_discharge.rate_relative.has_value());
    builder.SetBatteryDischargeRate(*battery_discharge.rate_relative);
  }
  if (metrics.cpu_usage.has_value()) {
    builder.SetCPUTimeMs(metrics.cpu_usage.value() *
                         interval_duration.InMilliseconds());
  }
#if BUILDFLAG(IS_MAC)
  builder.SetIdleWakeUps(metrics.idle_wakeups);
  builder.SetPackageExits(metrics.package_idle_wakeups);
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
  builder.SetDeviceSleptDuringInterval(interval_data.sleep_events);

  builder.Record(ukm_recorder);
}
