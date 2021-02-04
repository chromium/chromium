// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_metrics_recorder.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"

namespace performance_monitor {

namespace {

// If a process is consistently above this CPU utilization percentage over time,
// we consider it as high and may take action.
const float kHighCPUUtilizationThreshold = 90.0f;

}  // namespace

ProcessMetricsRecorder::ProcessMetricsRecorder(
    ProcessMonitor* process_monitor) {
  process_monitor_observation_.Observe(process_monitor);
}

ProcessMetricsRecorder::~ProcessMetricsRecorder() = default;

void ProcessMetricsRecorder::OnMetricsSampled(
    const ProcessMetadata& process_metadata,
    const ProcessMonitor::Metrics& metrics) {
  // We scale up to the equivalent of 64 CPU cores fully loaded. More than this
  // doesn't really matter, as we're already in a terrible place.
  const int kHistogramMin = 1;
  const int kHistogramMax = 6400;
  const int kHistogramBucketCount = 50;

#if defined(OS_WIN)
  const int kDiskUsageHistogramMin = 1;
  const int kDiskUsageHistogramMax = 200 * 1024 * 1024;  // 200 M/sec.
  const int kDiskUsageHistogramBucketCount = 50;
#endif

  // The histogram macros don't support variables as histogram names,
  // hence the macro duplication for each process type.
  switch (process_metadata.process_type) {
    case content::PROCESS_TYPE_BROWSER:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.BrowserProcess", metrics.cpu_usage,
          kHistogramMin, kHistogramMax, kHistogramBucketCount);
      // If CPU usage has consistently been above our threshold,
      // we *may* have an issue.
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.BrowserProcess",
                              true);
      }
#if defined(OS_WIN)
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageDisk.BrowserProcess", metrics.disk_usage,
          kDiskUsageHistogramMin, kDiskUsageHistogramMax,
          kDiskUsageHistogramBucketCount);
#endif
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
      UMA_HISTOGRAM_COUNTS_10000(
          "PerformanceMonitor.IdleWakeups.BrowserProcess",
          metrics.idle_wakeups);
#endif
#if defined(OS_MAC)
      UMA_HISTOGRAM_COUNTS_1000(
          "PerformanceMonitor.PackageExitIdleWakeups.BrowserProcess",
          metrics.package_idle_wakeups);
      UMA_HISTOGRAM_COUNTS_100000(
          "PerformanceMonitor.EnergyImpact.BrowserProcess",
          metrics.energy_impact);

#endif
      break;
    case content::PROCESS_TYPE_RENDERER:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.RendererProcess", metrics.cpu_usage,
          kHistogramMin, kHistogramMax, kHistogramBucketCount);
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.RendererProcess",
                              true);
      }
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
      UMA_HISTOGRAM_COUNTS_10000(
          "PerformanceMonitor.IdleWakeups.RendererProcess",
          metrics.idle_wakeups);
#endif
#if defined(OS_MAC)
      UMA_HISTOGRAM_COUNTS_1000(
          "PerformanceMonitor.PackageExitIdleWakeups.RendererProcess",
          metrics.package_idle_wakeups);
      UMA_HISTOGRAM_COUNTS_100000(
          "PerformanceMonitor.EnergyImpact.RendererProcess",
          metrics.energy_impact);

#endif

      break;
    case content::PROCESS_TYPE_GPU:
      UMA_HISTOGRAM_CUSTOM_COUNTS("PerformanceMonitor.AverageCPU.GPUProcess",
                                  metrics.cpu_usage, kHistogramMin,
                                  kHistogramMax, kHistogramBucketCount);
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold)
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.GPUProcess", true);
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
      UMA_HISTOGRAM_COUNTS_10000("PerformanceMonitor.IdleWakeups.GPUProcess",
                                 metrics.idle_wakeups);
#endif
#if defined(OS_MAC)
      UMA_HISTOGRAM_COUNTS_1000(
          "PerformanceMonitor.PackageExitIdleWakeups.GPUProcess",
          metrics.package_idle_wakeups);
      UMA_HISTOGRAM_COUNTS_100000("PerformanceMonitor.EnergyImpact.GPUProcess",
                                  metrics.energy_impact);

#endif

      break;
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      UMA_HISTOGRAM_CUSTOM_COUNTS("PerformanceMonitor.AverageCPU.PPAPIProcess",
                                  metrics.cpu_usage, kHistogramMin,
                                  kHistogramMax, kHistogramBucketCount);
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold)
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.PPAPIProcess", true);
      break;
    default:
      break;
  }

  switch (process_metadata.process_subtype) {
    case kProcessSubtypeUnknown:
      break;
    case kProcessSubtypePPAPIFlash:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.PPAPIFlashProcess", metrics.cpu_usage,
          kHistogramMin, kHistogramMax, kHistogramBucketCount);
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.PPAPIFlashProcess",
                              true);
      }
      break;
    case kProcessSubtypeExtensionPersistent:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.RendererExtensionPersistentProcess",
          metrics.cpu_usage, kHistogramMin, kHistogramMax,
          kHistogramBucketCount);
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN(
            "PerformanceMonitor.HighCPU.RendererExtensionPersistentProcess",
            true);
      }
      break;
    case kProcessSubtypeExtensionEvent:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.RendererExtensionEventProcess",
          metrics.cpu_usage, kHistogramMin, kHistogramMax,
          kHistogramBucketCount);
      if (metrics.cpu_usage > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN(
            "PerformanceMonitor.HighCPU.RendererExtensionEventProcess", true);
      }
      break;
  }
}

}  // namespace performance_monitor
