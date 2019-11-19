// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_metrics_history.h"

#include <limits>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "content/public/common/process_type.h"

#if defined(OS_MACOSX)
#include "content/public/browser/browser_child_process_host.h"
#endif

namespace performance_monitor {

// If a process is consistently above this CPU utilization percentage over time,
// we consider it as high and may take action.
const float kHighCPUUtilizationThreshold = 90.0f;

ProcessMetricsHistory::ProcessMetricsHistory() = default;

ProcessMetricsHistory::~ProcessMetricsHistory() = default;

void ProcessMetricsHistory::Initialize(
    const ProcessMetricsMetadata& process_data,
    int initial_update_sequence) {
  DCHECK_EQ(base::kNullProcessHandle, process_data_.handle);
  process_data_ = process_data;
  last_update_sequence_ = initial_update_sequence;

#if defined(OS_MACOSX)
  process_metrics_ = base::ProcessMetrics::CreateProcessMetrics(
      process_data_.handle,
      content::BrowserChildProcessHost::GetPortProvider());
#else
  process_metrics_ =
      base::ProcessMetrics::CreateProcessMetrics(process_data_.handle);
#endif
}

void ProcessMetricsHistory::SampleMetrics() {
  cpu_usage_ = process_metrics_->GetPlatformIndependentCPUUsage();
#if defined(OS_WIN)
  disk_usage_ = process_metrics_->GetDiskUsageBytesPerSecond();
#endif
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_AIX)
  idle_wakeups_ = process_metrics_->GetIdleWakeupsPerSecond();
#endif
#if defined(OS_MACOSX)
  package_idle_wakeups_ = process_metrics_->GetPackageIdleWakeupsPerSecond();
  energy_impact_ = process_metrics_->GetEnergyImpact();
#endif
}

void ProcessMetricsHistory::RunPerformanceTriggers() {
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
  switch (process_data_.process_type) {
    case content::PROCESS_TYPE_BROWSER:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.BrowserProcess", cpu_usage_,
          kHistogramMin, kHistogramMax, kHistogramBucketCount);
      // If CPU usage has consistently been above our threshold,
      // we *may* have an issue.
      if (cpu_usage_ > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.BrowserProcess",
                              true);
      }
#if defined(OS_WIN)
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageDisk.BrowserProcess", disk_usage_,
          kDiskUsageHistogramMin, kDiskUsageHistogramMax,
          kDiskUsageHistogramBucketCount);
#endif
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_AIX)
      UMA_HISTOGRAM_COUNTS_10000(
          "PerformanceMonitor.IdleWakeups.BrowserProcess", idle_wakeups_);
#endif
#if defined(OS_MACOSX)
      UMA_HISTOGRAM_COUNTS_1000(
          "PerformanceMonitor.PackageExitIdleWakeups.BrowserProcess",
          package_idle_wakeups_);
      UMA_HISTOGRAM_COUNTS_100000(
          "PerformanceMonitor.EnergyImpact.BrowserProcess", energy_impact_);

#endif
      break;
    case content::PROCESS_TYPE_RENDERER:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.RendererProcess", cpu_usage_,
          kHistogramMin, kHistogramMax, kHistogramBucketCount);
      if (cpu_usage_ > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.RendererProcess",
                              true);
      }
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_AIX)
      UMA_HISTOGRAM_COUNTS_10000(
          "PerformanceMonitor.IdleWakeups.RendererProcess", idle_wakeups_);
#endif
#if defined(OS_MACOSX)
      UMA_HISTOGRAM_COUNTS_1000(
          "PerformanceMonitor.PackageExitIdleWakeups.RendererProcess",
          package_idle_wakeups_);
      UMA_HISTOGRAM_COUNTS_100000(
          "PerformanceMonitor.EnergyImpact.RendererProcess", energy_impact_);

#endif

      break;
    case content::PROCESS_TYPE_GPU:
      UMA_HISTOGRAM_CUSTOM_COUNTS("PerformanceMonitor.AverageCPU.GPUProcess",
                                  cpu_usage_, kHistogramMin, kHistogramMax,
                                  kHistogramBucketCount);
      if (cpu_usage_ > kHighCPUUtilizationThreshold)
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.GPUProcess", true);
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_AIX)
      UMA_HISTOGRAM_COUNTS_10000("PerformanceMonitor.IdleWakeups.GPUProcess",
                                 idle_wakeups_);
#endif
#if defined(OS_MACOSX)
      UMA_HISTOGRAM_COUNTS_1000(
          "PerformanceMonitor.PackageExitIdleWakeups.GPUProcess",
          package_idle_wakeups_);
      UMA_HISTOGRAM_COUNTS_100000("PerformanceMonitor.EnergyImpact.GPUProcess",
                                  energy_impact_);

#endif

      break;
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      UMA_HISTOGRAM_CUSTOM_COUNTS("PerformanceMonitor.AverageCPU.PPAPIProcess",
                                  cpu_usage_, kHistogramMin, kHistogramMax,
                                  kHistogramBucketCount);
      if (cpu_usage_ > kHighCPUUtilizationThreshold)
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.PPAPIProcess", true);
      break;
    default:
      break;
  }

  switch (process_data_.process_subtype) {
    case kProcessSubtypeUnknown:
      break;
    case kProcessSubtypePPAPIFlash:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.PPAPIFlashProcess", cpu_usage_,
          kHistogramMin, kHistogramMax, kHistogramBucketCount);
      if (cpu_usage_ > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN("PerformanceMonitor.HighCPU.PPAPIFlashProcess",
                              true);
      }
      break;
    case kProcessSubtypeExtensionPersistent:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.RendererExtensionPersistentProcess",
          cpu_usage_, kHistogramMin, kHistogramMax, kHistogramBucketCount);
      if (cpu_usage_ > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN(
            "PerformanceMonitor.HighCPU.RendererExtensionPersistentProcess",
            true);
      }
      break;
    case kProcessSubtypeExtensionEvent:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "PerformanceMonitor.AverageCPU.RendererExtensionEventProcess",
          cpu_usage_, kHistogramMin, kHistogramMax, kHistogramBucketCount);
      if (cpu_usage_ > kHighCPUUtilizationThreshold) {
        UMA_HISTOGRAM_BOOLEAN(
            "PerformanceMonitor.HighCPU.RendererExtensionEventProcess", true);
      }
      break;
  }
}

}  // namespace performance_monitor
