// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"

namespace performance_monitor {

namespace {

// If a process is consistently above this CPU utilization percentage over time,
// we consider it as high and may take action.
const float kHighCPUUtilizationThreshold = 90.0f;

// CPU usage metrics are provided to this class as a double in the
// [0.0, number of cores * 100.0] range. The CPU usage is usually below 1%, so
// the histograms are reported with a 1/10000 granularity to make analyzing the
// data easier (otherwise almost all samples end up in the same [0, 1[ bucket).
constexpr int kCPUUsageFactor = 100;
// We scale up to the equivalent of 2 CPU cores fully loaded. More than this
// doesn't really matter, as we're already in a terrible place. This used to
// be capped at 64 cores but the data showed that this was way too much, the
// per process CPU usage really rarely exceeds 100% of one core.
constexpr int kCPUUsageHistogramMin = 1;
constexpr int kCPUUsageHistogramMax = 200 * kCPUUsageFactor;
constexpr int kCPUUsageHistogramBucketCount = 50;

}  // namespace

ProcessMetricsRecorder::ProcessMetricsRecorder(
    ProcessMonitor* process_monitor) {
  process_monitor_observation_.Observe(process_monitor);
}

ProcessMetricsRecorder::~ProcessMetricsRecorder() = default;

void ProcessMetricsRecorder::OnMetricsSampled(
    const ProcessMetadata& process_metadata,
    const ProcessMonitor::Metrics& metrics) {

#if defined(OS_WIN)
  constexpr int kDiskUsageHistogramMin = 1;
  constexpr int kDiskUsageHistogramMax = 200 * 1024 * 1024;  // 200 M/sec.
  constexpr int kDiskUsageHistogramBucketCount = 50;
#endif

  // The histogram macros don't support variables as histogram names,
  // hence the macro duplication for each process type.
  switch (process_metadata.process_type) {
    case content::PROCESS_TYPE_BROWSER:
      base::UmaHistogramCustomCounts(
          "PerformanceMonitor.AverageCPU2.BrowserProcess",
          metrics.cpu_usage * kCPUUsageFactor, kCPUUsageHistogramMin,
          kCPUUsageHistogramMax, kCPUUsageHistogramBucketCount);
      // If CPU usage has consistently been above our threshold,
      // we *may* have an issue.
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold) {
        base::UmaHistogramBoolean("PerformanceMonitor.HighCPU.BrowserProcess",
                                  true);
      }
#if defined(OS_WIN)
      base::UmaHistogramCustomCounts(
          "PerformanceMonitor.AverageDisk.BrowserProcess", metrics.disk_usage,
          kDiskUsageHistogramMin, kDiskUsageHistogramMax,
          kDiskUsageHistogramBucketCount);
#endif
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
      base::UmaHistogramCounts10000(
          "PerformanceMonitor.IdleWakeups.BrowserProcess",
          metrics.idle_wakeups);
#endif
#if defined(OS_MAC)
      base::UmaHistogramCounts1000(
          "PerformanceMonitor.PackageExitIdleWakeups.BrowserProcess",
          metrics.package_idle_wakeups);
      base::UmaHistogramCounts100000(
          "PerformanceMonitor.EnergyImpact.BrowserProcess",
          metrics.energy_impact);

#endif
      break;
    case content::PROCESS_TYPE_RENDERER:
      base::UmaHistogramCustomCounts(
          "PerformanceMonitor.AverageCPU2.RendererProcess2",
          metrics.cpu_usage * kCPUUsageFactor, kCPUUsageHistogramMin,
          kCPUUsageHistogramMax, kCPUUsageHistogramBucketCount);
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold) {
        base::UmaHistogramBoolean("PerformanceMonitor.HighCPU.RendererProcess",
                                  true);
      }
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
      base::UmaHistogramCounts10000(
          "PerformanceMonitor.IdleWakeups.RendererProcess",
          metrics.idle_wakeups);
#endif
#if defined(OS_MAC)
      base::UmaHistogramCounts1000(
          "PerformanceMonitor.PackageExitIdleWakeups.RendererProcess",
          metrics.package_idle_wakeups);
      base::UmaHistogramCounts100000(
          "PerformanceMonitor.EnergyImpact.RendererProcess",
          metrics.energy_impact);

#endif

      break;
    case content::PROCESS_TYPE_GPU:
      base::UmaHistogramCustomCounts(
          "PerformanceMonitor.AverageCPU2.GPUProcess",
          metrics.cpu_usage * kCPUUsageFactor, kCPUUsageHistogramMin,
          kCPUUsageHistogramMax, kCPUUsageHistogramBucketCount);
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold)
        base::UmaHistogramBoolean("PerformanceMonitor.HighCPU.GPUProcess",
                                  true);
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
      base::UmaHistogramCounts10000("PerformanceMonitor.IdleWakeups.GPUProcess",
                                    metrics.idle_wakeups);
#endif
#if defined(OS_MAC)
      base::UmaHistogramCounts1000(
          "PerformanceMonitor.PackageExitIdleWakeups.GPUProcess",
          metrics.package_idle_wakeups);
      base::UmaHistogramCounts100000(
          "PerformanceMonitor.EnergyImpact.GPUProcess", metrics.energy_impact);

#endif

      break;
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      base::UmaHistogramCustomCounts(
          "PerformanceMonitor.AverageCPU2.PPAPIProcess",
          metrics.cpu_usage * kCPUUsageFactor, kCPUUsageHistogramMin,
          kCPUUsageHistogramMax, kCPUUsageHistogramBucketCount);
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold)
        base::UmaHistogramBoolean("PerformanceMonitor.HighCPU.PPAPIProcess",
                                  true);
      break;
    case content::PROCESS_TYPE_UTILITY:
      base::UmaHistogramCustomCounts(
          "PerformanceMonitor.AverageCPU2.UtilityProcess", metrics.cpu_usage,
          kCPUUsageHistogramMin, kCPUUsageHistogramMax,
          kCPUUsageHistogramBucketCount);
      break;
    default:
      break;
  }

  switch (process_metadata.process_subtype) {
    case kProcessSubtypeUnknown:
      break;
    case kProcessSubtypePPAPIFlash:
      NOTREACHED() << "Flash isn't supported anymore.";
      break;
    case kProcessSubtypeExtensionPersistent:
      base::UmaHistogramCustomCounts(
          "PerformanceMonitor.AverageCPU2.RendererExtensionPersistentProcess",
          metrics.cpu_usage * kCPUUsageFactor, kCPUUsageHistogramMin,
          kCPUUsageHistogramMax, kCPUUsageHistogramBucketCount);
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold) {
        base::UmaHistogramBoolean(
            "PerformanceMonitor.HighCPU.RendererExtensionPersistentProcess",
            true);
      }
      break;
    case kProcessSubtypeExtensionEvent:
      base::UmaHistogramCustomCounts(
          "PerformanceMonitor.AverageCPU2.RendererExtensionEventProcess",
          metrics.cpu_usage * kCPUUsageFactor, kCPUUsageHistogramMin,
          kCPUUsageHistogramMax, kCPUUsageHistogramBucketCount);
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold) {
        base::UmaHistogramBoolean(
            "PerformanceMonitor.HighCPU.RendererExtensionEventProcess", true);
      }
      break;
    case kProcessSubtypeNetworkProcess:
      base::UmaHistogramCustomCounts(
          "PerformanceMonitor.AverageCPU2.NetworkProcess", metrics.cpu_usage,
          kCPUUsageHistogramMin, kCPUUsageHistogramMax,
          kCPUUsageHistogramBucketCount);
      break;
  }
}

void ProcessMetricsRecorder::OnAggregatedMetricsSampled(
    const ProcessMonitor::Metrics& metrics) {
  base::UmaHistogramCustomCounts("PerformanceMonitor.AverageCPU2.Total",
                                 metrics.cpu_usage * kCPUUsageFactor,
                                 kCPUUsageHistogramMin, kCPUUsageHistogramMax,
                                 kCPUUsageHistogramBucketCount);
}

}  // namespace performance_monitor
