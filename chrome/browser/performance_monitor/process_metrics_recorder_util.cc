// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_metrics_recorder_util.h"

#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#if defined(OS_MAC)
#include "chrome/browser/performance_monitor/resource_coalition_mac.h"
#endif

namespace performance_monitor {

namespace {

// CPU usage metrics are provided as a double in the [0.0, number of cores *
// 100.0] range. The CPU usage is usually below 1%, so the histograms are
// reported with a 1/10000 granularity to make analyzing the data easier
// (otherwise almost all samples end up in the same [0, 1[ bucket).
constexpr int kCPUUsageFactor = 100;
// We scale up to the equivalent of 2 CPU cores fully loaded. More than this
// doesn't really matter, as we're already in a terrible place. This used to
// be capped at 64 cores but the data showed that this was way too much, the
// per process CPU usage really rarely exceeds 100% of one core.
constexpr int kCPUUsageHistogramMin = 1;
constexpr int kCPUUsageHistogramMax = 200 * kCPUUsageFactor;
constexpr int kCPUUsageHistogramBucketCount = 50;

}  // namespace

void RecordProcessHistograms(const char* histogram_suffix,
                             const ProcessMonitor::Metrics& metrics) {
  base::UmaHistogramCustomCounts(
      base::JoinString({"PerformanceMonitor.AverageCPU2.", histogram_suffix},
                       ""),
      metrics.cpu_usage * kCPUUsageFactor, kCPUUsageHistogramMin,
      kCPUUsageHistogramMax, kCPUUsageHistogramBucketCount);
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
  base::UmaHistogramCounts10000(
      base::JoinString({"PerformanceMonitor.IdleWakeups.", histogram_suffix},
                       ""),
      metrics.idle_wakeups);
#endif
#if defined(OS_MAC)
  base::UmaHistogramCounts1000(
      base::JoinString(
          {"PerformanceMonitor.PackageExitIdleWakeups.", histogram_suffix}, ""),
      metrics.package_idle_wakeups);
  base::UmaHistogramCounts100000(
      base::JoinString({"PerformanceMonitor.EnergyImpact.", histogram_suffix},
                       ""),
      metrics.energy_impact);
#endif
}

#if defined(OS_MAC)
void RecordCoalitionData(const ProcessMonitor::Metrics& metrics,
                         const std::vector<const char*>& suffixes) {
  if (!metrics.coalition_data.has_value())
    return;

  // Calling this function with an empty suffix list is probably a mistake.
  DCHECK(!suffixes.empty());

  // TODO(crbug.com/1229884): Review the units and buckets once we have
  // sufficient data from the field.

  for (const char* scenario_suffix : suffixes) {
    // Suffixes are expected to be empty or starting by a period.
    DCHECK(::strlen(scenario_suffix) == 0U || scenario_suffix[0] == '.');
    // Report the CPU and GPU time histogram with the same approach as the one
    // used for the |AverageCPU2| histograms.

    // Used to change the percentage scale from [0,1] to [0,100], e.g. 0.111
    // 11.1% will be multiplied by 100 to be 11.1. This is necessary to reuse
    // the same constants as for the |AverageCPU2| histograms.
    constexpr int kPercentScaleFactor = 100;

    base::UmaHistogramCustomCounts(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.CPUTime2", scenario_suffix}),
        metrics.coalition_data->cpu_time_per_second * kPercentScaleFactor *
            kCPUUsageFactor,
        kCPUUsageHistogramMin, kCPUUsageHistogramMax,
        kCPUUsageHistogramBucketCount);
    // The GPU usage should always be <= 100% so use a lower value for the
    // histogram max.
    // TODO(sebmarchand): Confirm this from the data.
    base::UmaHistogramCustomCounts(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.GPUTime2", scenario_suffix}),
        metrics.coalition_data->gpu_time_per_second * kPercentScaleFactor *
            kCPUUsageFactor,
        kCPUUsageHistogramMin, 100 * kCPUUsageFactor,
        kCPUUsageHistogramBucketCount);

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
        scale_sample(metrics.coalition_data->interrupt_wakeups_per_second));
    base::UmaHistogramCounts1M(
        base::StrCat({"PerformanceMonitor.ResourceCoalition."
                      "PlatformIdleWakeupsPerSecond",
                      scenario_suffix}),
        scale_sample(metrics.coalition_data->platform_idle_wakeups_per_second));
    base::UmaHistogramCounts10M(
        base::StrCat({"PerformanceMonitor.ResourceCoalition.BytesReadPerSecond",
                      scenario_suffix}),
        scale_sample(metrics.coalition_data->bytesread_per_second));
    base::UmaHistogramCounts10M(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.BytesWrittenPerSecond",
             scenario_suffix}),
        scale_sample(metrics.coalition_data->byteswritten_per_second));

    // EnergyImpact is reported in centi-EI, so scaled up by a factor of 100
    // for the histogram recording.
    constexpr double kEnergyImpactScalingFactor = 100.0;
    base::UmaHistogramCounts100000(
        base::StrCat({"PerformanceMonitor.ResourceCoalition.EnergyImpact",
                      scenario_suffix}),
        std::roundl(metrics.coalition_data->energy_impact_per_second *
                    kEnergyImpactScalingFactor));

    constexpr int kNanoWattToMilliWatt = 1000 * 1000;
    // Use a maximum of 100 watts, or 100 * 1000 milliwatts.
    base::UmaHistogramCounts100000(
        base::StrCat(
            {"PerformanceMonitor.ResourceCoalition.Power", scenario_suffix}),
        std::roundl(metrics.coalition_data->power_nw / kNanoWattToMilliWatt));

    for (int i = 0;
         i < static_cast<int>(ResourceCoalition::QoSLevels::kMaxValue) + 1;
         ++i) {
      const char* qos_suffix = nullptr;
      switch (static_cast<ResourceCoalition::QoSLevels>(i)) {
        case ResourceCoalition::QoSLevels::kDefault:
          qos_suffix = "Default";
          break;
        case ResourceCoalition::QoSLevels::kMaintenance:
          qos_suffix = "Maintenance";
          break;
        case ResourceCoalition::QoSLevels::kBackground:
          qos_suffix = "Background";
          break;
        case ResourceCoalition::QoSLevels::kUtility:
          qos_suffix = "Utility";
          break;
        case ResourceCoalition::QoSLevels::kLegacy:
          qos_suffix = "Legacy";
          break;
        case ResourceCoalition::QoSLevels::kUserInitiated:
          qos_suffix = "UserInitiated";
          break;
        case ResourceCoalition::QoSLevels::kUserInteractive:
          qos_suffix = "UserInteractive";
          break;
      }
      base::UmaHistogramCustomCounts(
          base::StrCat({"PerformanceMonitor.ResourceCoalition.QoSLevel.",
                        qos_suffix, scenario_suffix}),
          std::roundl(metrics.coalition_data->qos_time_per_second[i] *
                      kPercentScaleFactor * kCPUUsageFactor),
          kCPUUsageHistogramMin, 100 * kCPUUsageFactor * 1000,
          kCPUUsageHistogramBucketCount);
    }
  }
}
#endif

}  // namespace performance_monitor
