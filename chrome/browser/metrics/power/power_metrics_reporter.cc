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
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/performance_monitor/process_metrics_recorder_util.h"
#include "chrome/browser/performance_monitor/process_monitor.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "components/power_metrics/resource_coalition_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

constexpr const char* kBatteryDischargeRateHistogramName =
    "Power.BatteryDischargeRate2";
constexpr const char* kBatteryDischargeModeHistogramName =
    "Power.BatteryDischargeMode";
constexpr const char* kBatterySamplingDelayHistogramName =
    "Power.BatterySamplingDelay";

// A trace event is emitted when CPU usage exceeds the 95th percentile.
// Canary 7 day aggregation ending on March 15th 2022 from "PerformanceMonitor
// .ResourceCoalition.CPUTime2_10sec.*"
const PowerMetricsReporter::ScenarioParams kVideoCaptureParams = {
    .histogram_suffix = ".VideoCapture",
    .short_interval_cpu_threshold = 1.8949,
    .trace_event_title = "High CPU - Video Capture",
};

const PowerMetricsReporter::ScenarioParams kFullscreenVideoParams = {
    .histogram_suffix = ".FullscreenVideo",
    .short_interval_cpu_threshold = 1.4513,
    .trace_event_title = "High CPU - Fullscreen Video",
};

const PowerMetricsReporter::ScenarioParams kEmbeddedVideoNoNavigationParams = {
    .histogram_suffix = ".EmbeddedVideo_NoNavigation",
    .short_interval_cpu_threshold = 1.5436,
    .trace_event_title = "High CPU - Embedded Video No Navigation",
};

const PowerMetricsReporter::ScenarioParams kEmbeddedVideoWithNavigationParams =
    {
        .histogram_suffix = ".EmbeddedVideo_WithNavigation",
        .short_interval_cpu_threshold = 1.9999,
        .trace_event_title = "High CPU - Embedded Video With Navigation",
};

const PowerMetricsReporter::ScenarioParams kAudioParams = {
    .histogram_suffix = ".Audio",
    .short_interval_cpu_threshold = 1.5110,
    .trace_event_title = "High CPU - Audio",
};

const PowerMetricsReporter::ScenarioParams kNavigationParams = {
    .histogram_suffix = ".Navigation",
    .short_interval_cpu_threshold = 1.9999,
    .trace_event_title = "High CPU - Navigation",
};

const PowerMetricsReporter::ScenarioParams kInteractionParams = {
    .histogram_suffix = ".Interaction",
    .short_interval_cpu_threshold = 1.2221,
    .trace_event_title = "High CPU - Interaction",
};

const PowerMetricsReporter::ScenarioParams kPassiveParams = {
    .histogram_suffix = ".Passive",
    .short_interval_cpu_threshold = 0.4736,
    .trace_event_title = "High CPU - Passive",
};

#if BUILDFLAG(IS_MAC)
const PowerMetricsReporter::ScenarioParams
    kAllTabsHiddenNoVideoCaptureOrAudioParams = {
        .histogram_suffix = ".AllTabsHidden_NoVideoCaptureOrAudio",
        .short_interval_cpu_threshold = 0.2095,
        .trace_event_title =
            "High CPU - All Tabs Hidden, No Video Capture or Audio",
};

const PowerMetricsReporter::ScenarioParams
    kAllTabsHiddenNoVideoCaptureOrAudioRecentParams = {
        .histogram_suffix = ".AllTabsHidden_NoVideoCaptureOrAudio_Recent",
        .short_interval_cpu_threshold = 0.3302,
        .trace_event_title =
            "High CPU - All Tabs Hidden, No Video Capture or Audio (Recent)",
};

const PowerMetricsReporter::ScenarioParams kAllTabsHiddenNoAudioParams = {
    .histogram_suffix = ".AllTabsHidden_Audio",
    .short_interval_cpu_threshold = 0.7036,
    .trace_event_title = "High CPU - All Tabs Hidden, No Audio",
};

const PowerMetricsReporter::ScenarioParams kAllTabsHiddenNoVideoCapture = {
    .histogram_suffix = ".AllTabsHidden_VideoCapture",
    .short_interval_cpu_threshold = 0.8679,
    .trace_event_title = "High CPU - All Tabs Hidden, Video Capture",
};

const PowerMetricsReporter::ScenarioParams kAllTabsHiddenZeroWindowParams = {
    .histogram_suffix = ".ZeroWindow",
    .short_interval_cpu_threshold = 0.0500,
    .trace_event_title = "High CPU - Zero Window",
};

const PowerMetricsReporter::ScenarioParams
    kAllTabsHiddenZeroWindowRecentParams = {
        .histogram_suffix = ".ZeroWindow_Recent",
        .short_interval_cpu_threshold = 0.0745,
        .trace_event_title = "High CPU - Zero Window (Recent)",
};

// Reports `proportion` of a time used to a histogram in permyriad (1/100 %).
// `proportion` is 0.5 if half a CPU core or half total GPU time is used. It can
// be above 1.0 if more than 1 CPU core is used. CPU and GPU usage is often
// below 1% so it's useful to report with 1/10000 granularity (otherwise most
// samples end up in the same bucket).
void UsageTimeHistogram(const std::string& histogram_name,
                        double proportion,
                        int max_proportion) {
  // Multiplicator to convert `proportion` to permyriad (1/100 %).
  // For example, 1.0 * kScaleFactor = 10000 1/100 % = 100 %.
  constexpr int kScaleFactor = 100 * 100;

  base::UmaHistogramCustomCounts(
      histogram_name, std::lroundl(proportion * kScaleFactor),
      /* min=*/1, /* exclusive_max=*/max_proportion * kScaleFactor,
      /* buckets=*/50);
}

// Max proportion for CPU time histograms. This used to be 64 but was reduced to
// 2 because data shows that less than 0.2% of samples are above that.
constexpr int kMaxCPUProportion = 2;

// Max proportion for GPU time histograms. It's not possible to use more than
// 100% of total GPU time.
constexpr int kMaxGPUProportion = 1;
#endif  // BUILDFLAG(IS_MAC)

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

const PowerMetricsReporter::ScenarioParams& GetScenarioParamsWithVisibleWindow(
    const UsageScenarioDataStore::IntervalData& interval_data) {
  // The order of the conditions is important. See the full description of each
  // scenario in the histograms.xml file.
  DCHECK_GT(interval_data.max_visible_window_count, 0);

  if (!interval_data.time_capturing_video.is_zero())
    return kVideoCaptureParams;
  if (!interval_data.time_playing_video_full_screen_single_monitor.is_zero())
    return kFullscreenVideoParams;
  if (!interval_data.time_playing_video_in_visible_tab.is_zero()) {
    // Note: UKM data reveals that navigations are infrequent when a video is
    // playing in fullscreen, when video is captured or when audio is playing.
    // For that reason, there is no distinct suffix for navigation vs. no
    // navigation in these cases.
    if (interval_data.top_level_navigation_count == 0)
      return kEmbeddedVideoNoNavigationParams;
    return kEmbeddedVideoWithNavigationParams;
  }
  if (!interval_data.time_playing_audio.is_zero())
    return kAudioParams;
  if (interval_data.top_level_navigation_count > 0)
    return kNavigationParams;
  if (interval_data.user_interaction_count > 0)
    return kInteractionParams;
  return kPassiveParams;
}

// Helper function for GetLongIntervalSuffixes().
const char* GetLongIntervalScenarioSuffix(
    const UsageScenarioDataStore::IntervalData& interval_data) {
  // The order of the conditions is important. See the full description of each
  // scenario in the histograms.xml file.
  if (interval_data.max_tab_count == 0)
    return ".ZeroWindow";
  if (interval_data.max_visible_window_count == 0) {
    if (!interval_data.time_capturing_video.is_zero())
      return ".AllTabsHidden_VideoCapture";
    if (!interval_data.time_playing_audio.is_zero())
      return ".AllTabsHidden_Audio";
    return ".AllTabsHidden_NoVideoCaptureOrAudio";
  }
  return GetScenarioParamsWithVisibleWindow(interval_data).histogram_suffix;
}

// Returns suffixes to use for histograms related to a long interval described
// by `interval_data`.
std::vector<const char*> GetLongIntervalSuffixes(
    const UsageScenarioDataStore::IntervalData& interval_data) {
  // Histograms are recorded without suffix and with a scenario-specific
  // suffix.
  return {"", GetLongIntervalScenarioSuffix(interval_data)};
}

}  // namespace

#if BUILDFLAG(IS_MAC)
const PowerMetricsReporter::ScenarioParams&
PowerMetricsReporter::GetShortIntervalScenarioParams(
    const UsageScenarioDataStore::IntervalData& short_interval_data,
    const UsageScenarioDataStore::IntervalData& pre_interval_data) {
  // The order of the conditions is important. See the full description of each
  // scenario in the histograms.xml file.
  if (short_interval_data.max_tab_count == 0) {
    if (pre_interval_data.max_tab_count != 0)
      return kAllTabsHiddenZeroWindowRecentParams;
    return kAllTabsHiddenZeroWindowParams;
  }
  if (short_interval_data.max_visible_window_count == 0) {
    if (!short_interval_data.time_capturing_video.is_zero())
      return kAllTabsHiddenNoVideoCapture;
    if (!short_interval_data.time_playing_audio.is_zero())
      return kAllTabsHiddenNoAudioParams;
    if (pre_interval_data.max_visible_window_count != 0 ||
        !pre_interval_data.time_capturing_video.is_zero() ||
        !pre_interval_data.time_playing_audio.is_zero()) {
      return kAllTabsHiddenNoVideoCaptureOrAudioRecentParams;
    }
    return kAllTabsHiddenNoVideoCaptureOrAudioParams;
  }

  return GetScenarioParamsWithVisibleWindow(short_interval_data);
}
#endif  // BUILDFLAG(IS_MAC)

PowerMetricsReporter::PowerMetricsReporter(
    UsageScenarioDataStore* short_usage_scenario_data_store,
    UsageScenarioDataStore* long_usage_scenario_data_store,
    std::unique_ptr<BatteryLevelProvider> battery_level_provider
#if BUILDFLAG(IS_MAC)
    ,
    std::unique_ptr<CoalitionResourceUsageProvider>
        coalition_resource_usage_provider
#endif  // BUILDFLAG(IS_MAC)
    )
    : short_usage_scenario_data_store_(short_usage_scenario_data_store),
      long_usage_scenario_data_store_(long_usage_scenario_data_store),
      battery_level_provider_(std::move(battery_level_provider))
#if BUILDFLAG(IS_MAC)
      ,
      short_interval_timer_(
          FROM_HERE,
          performance_monitor::ProcessMonitor::kGatherInterval -
              kShortIntervalDuration,
          base::BindRepeating(&PowerMetricsReporter::OnShortIntervalBegin,
                              base::Unretained(this))),
      coalition_resource_usage_provider_(
          std::move(coalition_resource_usage_provider))
#endif  // BUILDFLAG(IS_MAC)
{
  DCHECK(ProcessMonitor::Get());
  ProcessMonitor::Get()->AddObserver(this);

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
        
  // Unretained() is safe here because |this| outlive |battery_level_provider_|.
  battery_level_provider_->GetBatteryState(
      base::BindOnce(&PowerMetricsReporter::OnFirstBatteryStateSampled,
                     base::Unretained(this)));

#if BUILDFLAG(IS_MAC)
  iopm_power_source_sampling_event_source_.Start(
      base::BindRepeating(&PowerMetricsReporter::OnIOPMPowerSourceSamplingEvent,
                          base::Unretained(this)));

  coalition_resource_usage_provider_->Init();
  short_interval_timer_.Reset();
#endif
}

PowerMetricsReporter::~PowerMetricsReporter() {
  if (auto* process_monitor = ProcessMonitor::Get()) {
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
    const ProcessMonitor::Metrics& metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Unretained() is safe here because |this| outlive |battery_level_provider_|.
  battery_level_provider_->GetBatteryState(base::BindOnce(
      &PowerMetricsReporter::OnBatteryAndAggregatedProcessMetricsSampled,
      base::Unretained(this), metrics,
      /* battery_sample_begin_time=*/base::TimeTicks::Now()));

#if BUILDFLAG(IS_MAC)
  short_interval_timer_.Reset();
#endif  // BUILDFLAG(IS_MAC)
}

std::vector<const char*>
PowerMetricsReporter::GetLongIntervalSuffixesForTesting(
    const UsageScenarioDataStore::IntervalData& interval_data) {
  return GetLongIntervalSuffixes(interval_data);
}

void PowerMetricsReporter::ReportLongIntervalHistograms(
    const UsageScenarioDataStore::IntervalData& interval_data,
    const ProcessMonitor::Metrics& aggregated_process_metrics,
    base::TimeDelta interval_duration,
    BatteryDischarge battery_discharge
#if BUILDFLAG(IS_MAC)
    ,
    const absl::optional<CoalitionResourceUsageRate>&
        coalition_resource_usage_rate
#endif
) {
  const auto suffixes = GetLongIntervalSuffixes(interval_data);
  ReportAggregatedProcessMetricsHistograms(aggregated_process_metrics,
                                           suffixes);

  ReportBatteryHistograms(interval_duration, battery_discharge, suffixes);
#if BUILDFLAG(IS_MAC)
  if (coalition_resource_usage_rate.has_value()) {
    ReportResourceCoalitionHistograms(coalition_resource_usage_rate.value(),
                                      suffixes);
  }
#endif
}

#if BUILDFLAG(IS_MAC)
void PowerMetricsReporter::ReportShortIntervalHistograms(
    const char* scenario_suffix,
    absl::optional<CoalitionResourceUsageRate> coalition_resource_usage_rate) {
  if (!coalition_resource_usage_rate.has_value())
    return;

  for (const char* suffix : {"", scenario_suffix}) {
    UsageTimeHistogram(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.CPUTime2_10sec", suffix}),
        coalition_resource_usage_rate->cpu_time_per_second, kMaxCPUProportion);
  }
}

void PowerMetricsReporter::MaybeEmitHighCPUTraceEvent(
    const PowerMetricsReporter::ScenarioParams& short_interval_scenario_params,
    absl::optional<CoalitionResourceUsageRate> coalition_resource_usage_rate) {
  if (!coalition_resource_usage_rate.has_value())
    return;

  if (coalition_resource_usage_rate->cpu_time_per_second >=
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
#endif  // BUILDFLAG(IS_MAC)

void PowerMetricsReporter::ReportBatteryHistograms(
    base::TimeDelta interval_duration,
    BatteryDischarge battery_discharge,
    const std::vector<const char*>& suffixes) {
  // Ratio by which the time elapsed can deviate from
  // |ProcessMonitor::kGatherInterval| without invalidating this sample.
  constexpr double kTolerableTimeElapsedRatio = 0.10;
  constexpr double kTolerablePositiveDrift = (1. + kTolerableTimeElapsedRatio);
  constexpr double kTolerableNegativeDrift = (1. - kTolerableTimeElapsedRatio);

  if (battery_discharge.mode == BatteryDischargeMode::kDischarging &&
      interval_duration >
          ProcessMonitor::kGatherInterval * kTolerablePositiveDrift) {
    // Too much time passed since the last record. Either the task took
    // too long to get executed or system sleep took place.
    battery_discharge.mode = BatteryDischargeMode::kInvalidInterval;
  }

  if (battery_discharge.mode == BatteryDischargeMode::kDischarging &&
      interval_duration <
          ProcessMonitor::kGatherInterval * kTolerableNegativeDrift) {
    // The recording task executed too early after the previous one, possibly
    // because the previous task took too long to execute.
    battery_discharge.mode = BatteryDischargeMode::kInvalidInterval;
  }

  for (const char* suffix : suffixes) {
    base::UmaHistogramEnumeration(
        base::StrCat({kBatteryDischargeModeHistogramName, suffix}),
        battery_discharge.mode);

    if (battery_discharge.mode == BatteryDischargeMode::kDischarging) {
      DCHECK(battery_discharge.rate.has_value());
      base::UmaHistogramCounts1000(
          base::StrCat({kBatteryDischargeRateHistogramName, suffix}),
          *battery_discharge.rate);
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

void PowerMetricsReporter::OnBatteryAndAggregatedProcessMetricsSampled(
    const ProcessMonitor::Metrics& aggregated_process_metrics,
    base::TimeTicks battery_sample_begin_time,
    const BatteryLevelProvider::BatteryState& battery_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::TimeTicks now = base::TimeTicks::Now();

  // Report time it took to sample the battery state.
  base::UmaHistogramMicrosecondsTimes(kBatterySamplingDelayHistogramName,
                                      now - battery_sample_begin_time);

  // Evaluate the interval duration.
  base::TimeDelta interval_duration = now - interval_begin_;
  interval_begin_ = now;

  // Evaluate battery discharge mode and rate.
  auto battery_discharge =
      GetBatteryDischargeDuringInterval(battery_state, interval_duration);

  // Get usage scenario data.
  auto long_interval_data =
      long_usage_scenario_data_store_->ResetIntervalData();

#if BUILDFLAG(IS_MAC)
  // Sample coalition resource usage rate.
  absl::optional<power_metrics::CoalitionResourceUsageRate>
      short_interval_resource_usage_rate;
  absl::optional<power_metrics::CoalitionResourceUsageRate>
      long_interval_resource_usage_rate;
  coalition_resource_usage_provider_->EndIntervals(
      &short_interval_resource_usage_rate, &long_interval_resource_usage_rate);
#endif  // BUILDFLAG(IS_MAC)

  // Report UKMs.
  ReportUKMs(long_interval_data, aggregated_process_metrics, interval_duration,
             battery_discharge);

  // Report histograms.
  ReportLongIntervalHistograms(long_interval_data, aggregated_process_metrics,
                               interval_duration, battery_discharge
#if BUILDFLAG(IS_MAC)
                               ,
                               long_interval_resource_usage_rate
#endif
  );
#if BUILDFLAG(IS_MAC)
  auto short_interval_data =
      short_usage_scenario_data_store_->ResetIntervalData();
  const PowerMetricsReporter::ScenarioParams short_interval_scenario_params =
      GetShortIntervalScenarioParams(short_interval_data, long_interval_data);

  ReportShortIntervalHistograms(short_interval_scenario_params.histogram_suffix,
                                short_interval_resource_usage_rate);
  MaybeEmitHighCPUTraceEvent(short_interval_scenario_params,
                             short_interval_resource_usage_rate);
#endif  // BUILDFLAG(IS_MAC)

  if (on_battery_sampled_for_testing_)
    std::move(on_battery_sampled_for_testing_).Run();
}

// static
void PowerMetricsReporter::ReportAggregatedProcessMetricsHistograms(
    const ProcessMonitor::Metrics& aggregated_process_metrics,
    const std::vector<const char*>& suffixes) {
  for (const char* suffix : suffixes) {
    std::string complete_suffix = base::StrCat({"Total", suffix});
    performance_monitor::RecordProcessHistograms(complete_suffix.c_str(),
                                                 aggregated_process_metrics);
  }
}

#if BUILDFLAG(IS_MAC)
// static
void PowerMetricsReporter::ReportResourceCoalitionHistograms(
    const power_metrics::CoalitionResourceUsageRate& rate,
    const std::vector<const char*>& suffixes) {
  // Calling this function with an empty suffix list is probably a mistake.
  DCHECK(!suffixes.empty());

  // TODO(crbug.com/1229884): Review the units and buckets once we have
  // sufficient data from the field.

  for (const char* scenario_suffix : suffixes) {
    // Suffixes are expected to be empty or starting by a period.
    DCHECK(::strlen(scenario_suffix) == 0U || scenario_suffix[0] == '.');

    UsageTimeHistogram(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.CPUTime2", scenario_suffix}),
        rate.cpu_time_per_second, kMaxCPUProportion);
    UsageTimeHistogram(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.GPUTime2", scenario_suffix}),
        rate.gpu_time_per_second, kMaxGPUProportion);

    // Report the metrics based on a count (e.g. wakeups) with a millievent/sec
    // granularity. In theory it doesn't make much sense to talk about a
    // milliwakeups but the wakeup rate should ideally be lower than one per
    // second in some scenarios and this will provide more granularity.
    constexpr int kMilliFactor = 1000;
    auto scale_sample = [](double sample) -> int {
      // Round the sample to the nearest integer value.
      return std::roundl(sample * kMilliFactor);
    };
    base::UmaHistogramCounts1M(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.InterruptWakeupsPerSecond",
             scenario_suffix}),
        scale_sample(rate.interrupt_wakeups_per_second));
    base::UmaHistogramCounts1M(
        base::StrCat({"PerformanceMonitor.ResourceCoalition."
                      "PlatformIdleWakeupsPerSecond",
                      scenario_suffix}),
        scale_sample(rate.platform_idle_wakeups_per_second));
    base::UmaHistogramCounts10M(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.BytesReadPerSecond2",
             scenario_suffix}),
        rate.bytesread_per_second);
    base::UmaHistogramCounts10M(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.BytesWrittenPerSecond2",
             scenario_suffix}),
        rate.byteswritten_per_second);

    // EnergyImpact is reported in centi-EI, so scaled up by a factor of 100
    // for the histogram recording.
    if (rate.energy_impact_per_second.has_value()) {
      constexpr double kEnergyImpactScalingFactor = 100.0;
      base::UmaHistogramCounts100000(
          base::StrCat({"PerformanceMonitor.ResourceCoalition.EnergyImpact",
                        scenario_suffix}),
          std::roundl(rate.energy_impact_per_second.value() *
                      kEnergyImpactScalingFactor));
    }

    // As of Feb 2, 2022, the value of `rate->power_nw` is always zero on Intel.
    // Don't report it to avoid polluting the data.
    if (base::mac::GetCPUType() == base::mac::CPUType::kArm) {
      constexpr int kMilliWattPerWatt = 1000;
      constexpr int kNanoWattPerMilliWatt = 1000 * 1000;
      // The maximum is 10 watts, which is larger than the 99.99th percentile
      // as of Feb 2, 2022.
      base::UmaHistogramCustomCounts(
          base::StrCat(
              {"PerformanceMonitor.ResourceCoalition.Power2", scenario_suffix}),
          std::roundl(rate.power_nw / kNanoWattPerMilliWatt),
          /* min=*/1, /* exclusive_max=*/10 * kMilliWattPerWatt,
          /* buckets=*/50);
    }

    auto record_qos_level = [&](size_t index, const char* qos_suffix) {
      UsageTimeHistogram(
          base::StrCat({"PerformanceMonitor.ResourceCoalition.QoSLevel.",
                        qos_suffix, scenario_suffix}),
          rate.qos_time_per_second[index], kMaxCPUProportion);
    };

    record_qos_level(THREAD_QOS_DEFAULT, "Default");
    record_qos_level(THREAD_QOS_MAINTENANCE, "Maintenance");
    record_qos_level(THREAD_QOS_BACKGROUND, "Background");
    record_qos_level(THREAD_QOS_UTILITY, "Utility");
    record_qos_level(THREAD_QOS_LEGACY, "Legacy");
    record_qos_level(THREAD_QOS_USER_INITIATED, "UserInitiated");
    record_qos_level(THREAD_QOS_USER_INTERACTIVE, "UserInteractive");
  }
}
#endif  // BUILDFLAG(IS_MAC)

void PowerMetricsReporter::ReportUKMs(
    const UsageScenarioDataStore::IntervalData& interval_data,
    const ProcessMonitor::Metrics& metrics,
    base::TimeDelta interval_duration,
    BatteryDischarge battery_discharge) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

PowerMetricsReporter::BatteryDischarge
PowerMetricsReporter::GetBatteryDischargeDuringInterval(
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

#if BUILDFLAG(IS_MAC)
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

#if BUILDFLAG(IS_MAC)
void PowerMetricsReporter::OnShortIntervalBegin() {
  short_interval_begin_time_ = base::TimeTicks::Now();
  short_usage_scenario_data_store_->ResetIntervalData();
  coalition_resource_usage_provider_->StartShortInterval();
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
