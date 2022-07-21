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

#if BUILDFLAG(IS_MAC)
#include "components/power_metrics/resource_coalition_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
constexpr const char* kBatterySamplingDelayHistogramName =
    "Power.BatterySamplingDelay";

bool IsWithinTolerance(base::TimeDelta value,
                       base::TimeDelta expected,
                       base::TimeDelta tolerance) {
  return (value - expected).magnitude() < tolerance;
}
#endif

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
    case MonitoredProcessType::kPPAPIPlugin:
      return "PPAPIProcess";
    case MonitoredProcessType::kUtility:
      return "UtilityProcess";
    case MonitoredProcessType::kNetwork:
      return "NetworkProcess";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

PowerMetricsReporter::PowerMetricsReporter(
    ProcessMonitor* process_monitor,
    UsageScenarioDataStore* short_usage_scenario_data_store,
    UsageScenarioDataStore* long_usage_scenario_data_store
#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
    ,
    std::unique_ptr<BatteryLevelProvider> battery_level_provider
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()
#if BUILDFLAG(IS_MAC)
    ,
    std::unique_ptr<CoalitionResourceUsageProvider>
        coalition_resource_usage_provider
#endif  // BUILDFLAG(IS_MAC)
    )
    : process_monitor_(process_monitor),
      short_usage_scenario_data_store_(short_usage_scenario_data_store),
      long_usage_scenario_data_store_(long_usage_scenario_data_store)
#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
      ,
      battery_level_provider_(std::move(battery_level_provider))
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()
#if BUILDFLAG(IS_MAC)
      ,
      coalition_resource_usage_provider_(
          std::move(coalition_resource_usage_provider))
#endif  // BUILDFLAG(IS_MAC)
{
  if (!short_usage_scenario_data_store_) {
    short_usage_scenario_tracker_ = std::make_unique<UsageScenarioTracker>();
    short_usage_scenario_data_store_ =
        short_usage_scenario_tracker_->data_store();
  }

  if (!long_usage_scenario_data_store_) {
    long_usage_scenario_tracker_ = std::make_unique<UsageScenarioTracker>();
    long_usage_scenario_data_store_ =
        long_usage_scenario_tracker_->data_store();
  }

  interval_begin_ = base::TimeTicks::Now();

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
  // Unretained() is safe here because |this| outlive |battery_level_provider_|.
  battery_level_provider_->GetBatteryState(
      base::BindOnce(&PowerMetricsReporter::OnFirstBatteryStateSampled,
                     base::Unretained(this)));
#endif

#if BUILDFLAG(IS_MAC)
  iopm_power_source_sampling_event_source_.Start(
      base::BindRepeating(&PowerMetricsReporter::OnIOPMPowerSourceSamplingEvent,
                          base::Unretained(this)));

  coalition_resource_usage_provider_->Init();
#endif

  StartNextLongInterval();
}

PowerMetricsReporter::~PowerMetricsReporter() = default;

// static
int64_t PowerMetricsReporter::GetBucketForSampleForTesting(
    base::TimeDelta value) {
  return GetBucketForSample(value);
}

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
void PowerMetricsReporter::OnFirstBatteryStateSampled(
    const absl::optional<BatteryLevelProvider::BatteryState>& battery_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  battery_state_ = battery_state;
}
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

void PowerMetricsReporter::StartNextLongInterval() {
#if BUILDFLAG(IS_MAC)
  // On Mac, set the timer for 10 seconds before the end of the long interval to
  // start the short interval.
  interval_timer_.Start(
      FROM_HERE,
      kLongPowerMetricsIntervalDuration - kShortPowerMetricsIntervalDuration,
      base::BindOnce(&PowerMetricsReporter::OnShortIntervalBegin,
                     base::Unretained(this)));
#else
  interval_timer_.Start(FROM_HERE, kLongPowerMetricsIntervalDuration,
                        base::BindOnce(&PowerMetricsReporter::OnLongIntervalEnd,
                                       base::Unretained(this)));
#endif
}

#if BUILDFLAG(IS_MAC)
void PowerMetricsReporter::OnShortIntervalBegin() {
  short_interval_begin_time_ = base::TimeTicks::Now();
  short_usage_scenario_data_store_->ResetIntervalData();
  coalition_resource_usage_provider_->StartShortInterval();

  interval_timer_.Start(
      FROM_HERE, kShortPowerMetricsIntervalDuration,
      base::BindRepeating(&PowerMetricsReporter::OnLongIntervalEnd,
                          base::Unretained(this)));
}
#endif  // BUILDFLAG(IS_MAC)

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
#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
  // Note: The use of `Unretained()` is safe here because |this| outlives
  //       |battery_level_provider_|.
  battery_level_provider_->GetBatteryState(base::BindOnce(
      &PowerMetricsReporter::OnBatteryAndAggregatedProcessMetricsSampled,
      base::Unretained(this), metrics, interval_duration,
      /*battery_sample_begin_time=*/now));
#else
  ReportMetrics(interval_duration, metrics);
#endif
}

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
void PowerMetricsReporter::OnBatteryAndAggregatedProcessMetricsSampled(
    const ProcessMonitor::Metrics& aggregated_process_metrics,
    base::TimeDelta interval_duration,
    base::TimeTicks battery_sample_begin_time,
    const absl::optional<BatteryLevelProvider::BatteryState>&
        new_battery_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Report time it took to sample the battery state.
  base::UmaHistogramMicrosecondsTimes(
      kBatterySamplingDelayHistogramName,
      base::TimeTicks::Now() - battery_sample_begin_time);

  // Evaluate battery discharge mode and rate.
  auto previous_battery_state =
      std::exchange(battery_state_, new_battery_state);
  auto battery_discharge = GetBatteryDischargeDuringInterval(
      previous_battery_state, new_battery_state, interval_duration);

  ReportMetrics(interval_duration, aggregated_process_metrics,
                battery_discharge);
}
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

void PowerMetricsReporter::ReportMetrics(
    base::TimeDelta interval_duration,
    const ProcessMonitor::Metrics& aggregated_process_metrics
#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
    ,
    BatteryDischarge battery_discharge
#endif
) {
  // Get usage scenario data.
  auto long_interval_data =
      long_usage_scenario_data_store_->ResetIntervalData();

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

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
  // Report UKMs.
  ReportUKMs(long_interval_data, aggregated_process_metrics, interval_duration,
             battery_discharge);

  // Ratio by which the time elapsed can deviate from
  // |kLongPowerMetricsIntervalDuration| without invalidating this sample.
  // TODO(pmonette): Change to DCHECK after ensuring this never triggers.
  CHECK_GE(interval_duration, kLongPowerMetricsIntervalDuration);
  constexpr double kTolerableTimeElapsedRatio = 0.10;
  if (battery_discharge.mode == BatteryDischargeMode::kDischarging &&
      !IsWithinTolerance(
          interval_duration, kLongPowerMetricsIntervalDuration,
          kLongPowerMetricsIntervalDuration * kTolerableTimeElapsedRatio)) {
    battery_discharge.mode = BatteryDischargeMode::kInvalidInterval;
  }
  ReportBatteryHistograms(interval_duration, battery_discharge,
                          long_interval_suffixes);
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

#if BUILDFLAG(IS_MAC)
  // Sample coalition resource usage rate.
  absl::optional<power_metrics::CoalitionResourceUsageRate>
      short_interval_resource_usage_rate;
  absl::optional<power_metrics::CoalitionResourceUsageRate>
      long_interval_resource_usage_rate;
  coalition_resource_usage_provider_->EndIntervals(
      &short_interval_resource_usage_rate, &long_interval_resource_usage_rate);

  // Report resource coalition histograms for the long interval.
  if (long_interval_resource_usage_rate.has_value()) {
    ReportResourceCoalitionHistograms(long_interval_resource_usage_rate.value(),
                                      long_interval_suffixes);
  }

  // Then do it for the short interval.
  if (short_interval_resource_usage_rate.has_value()) {
    auto short_interval_data =
        short_usage_scenario_data_store_->ResetIntervalData();
    const ScenarioParams short_interval_scenario_params =
        GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

    base::UmaHistogramEnumeration(
        "PerformanceMonitor.UsageScenario.ShortInterval",
        short_interval_scenario_params.scenario);

    ReportShortIntervalHistograms(
        short_interval_scenario_params.histogram_suffix,
        short_interval_resource_usage_rate.value());
    MaybeEmitHighCPUTraceEvent(short_interval_scenario_params,
                               short_interval_resource_usage_rate.value());
  }
#endif  // BUILDFLAG(IS_MAC)

  StartNextLongInterval();
}

#if HAS_BATTERY_LEVEL_PROVIDER_IMPL()
void PowerMetricsReporter::ReportUKMs(
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
  builder.SetBatteryDischargeMode(static_cast<int64_t>(battery_discharge.mode));
  if (battery_discharge.mode == BatteryDischargeMode::kDischarging) {
    DCHECK(battery_discharge.rate.has_value());
    builder.SetBatteryDischargeRate(*battery_discharge.rate);
  }
  builder.SetCPUTimeMs(metrics.cpu_usage * interval_duration.InMilliseconds());
#if BUILDFLAG(IS_MAC)
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
  builder.SetDeviceSleptDuringInterval(interval_data.sleep_events);

  builder.Record(ukm_recorder);
}
#endif  // HAS_BATTERY_LEVEL_PROVIDER_IMPL()

#if BUILDFLAG(IS_MAC)
void PowerMetricsReporter::MaybeEmitHighCPUTraceEvent(
    const ScenarioParams& short_interval_scenario_params,
    const CoalitionResourceUsageRate& coalition_resource_usage_rate) {
  if (coalition_resource_usage_rate.cpu_time_per_second >=
      short_interval_scenario_params.short_interval_cpu_threshold) {
    const base::TimeTicks now = base::TimeTicks::Now();

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "browser", short_interval_scenario_params.trace_event_title,
        TRACE_ID_LOCAL(this), short_interval_begin_time_);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "browser", short_interval_scenario_params.trace_event_title,
        TRACE_ID_LOCAL(this), now);
  }
  short_interval_begin_time_ = base::TimeTicks();
}

void PowerMetricsReporter::OnIOPMPowerSourceSamplingEvent() {
  base::TimeTicks now_ticks = base::TimeTicks::Now();

  if (!last_event_time_ticks_) {
    last_event_time_ticks_ = now_ticks;
    return;
  }

  // The delta is expected to be almost always 60 seconds. Split the buckets for
  // 0.2s granularity (10s interval with 50 buckets + 1 underflow bucket + 1
  // overflow bucket) around that value.
  base::TimeDelta sampling_event_delta = now_ticks - *last_event_time_ticks_;
  base::HistogramBase* histogram = base::LinearHistogram::FactoryTimeGet(
      "Power.IOPMPowerSource.SamplingEventDelta",
      /*min=*/base::Seconds(55), /*max=*/base::Seconds(65), /*buckets=*/52,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(sampling_event_delta);
  *last_event_time_ticks_ = now_ticks;

  // Same as the above but using a range that starts from zero and significantly
  // goes beyond the expected mean time of |sampling_event_delta| (which is 60
  // seconds.).
  base::UmaHistogramMediumTimes(
      "Power.IOPMPowerSource.SamplingEventDelta.MediumTimes",
      sampling_event_delta);
}
#endif  // BUILDFLAG(IS_MAC)
