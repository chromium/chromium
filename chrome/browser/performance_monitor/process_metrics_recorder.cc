// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

namespace performance_monitor {

namespace {

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
      base::JoinString({"PerformanceMonitor.IdleWakeUps.", histogram_suffix},
                       ""),
      metrics.idle_wakeups);
#endif
#if defined(OS_MAC)
  base::UmaHistogramCounts1000(
      base::JoinString(
          {"PerformanceMonitor.PackageExitIdleWakeUps.", histogram_suffix}, ""),
      metrics.package_idle_wakeups);
  base::UmaHistogramCounts100000(
      base::JoinString({"PerformanceMonitor.EnergyImpact.", histogram_suffix},
                       ""),
      metrics.energy_impact);
#endif
}

}  // namespace

ProcessMetricsRecorder::ProcessMetricsRecorder(
    ProcessMonitor* process_monitor) {
  process_monitor_observation_.Observe(process_monitor);
}

ProcessMetricsRecorder::~ProcessMetricsRecorder() = default;

void ProcessMetricsRecorder::OnMetricsSampled(
    const ProcessMetadata& process_metadata,
    const ProcessMonitor::Metrics& metrics) {
  // The histogram macros don't support variables as histogram names,
  // hence the macro duplication for each process type.
  switch (process_metadata.process_type) {
    case content::PROCESS_TYPE_BROWSER:
      RecordProcessHistograms("BrowserProcess", metrics);
      break;
    case content::PROCESS_TYPE_RENDERER:
      RecordProcessHistograms("RendererProcess", metrics);
      break;
    case content::PROCESS_TYPE_GPU:
      RecordProcessHistograms("GPUProcess", metrics);
      break;
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      RecordProcessHistograms("PPAPIProcess", metrics);
      break;
    case content::PROCESS_TYPE_UTILITY:
      RecordProcessHistograms("UtilityProcess", metrics);
      break;
    default:
      break;
  }

  switch (process_metadata.process_subtype) {
    case kProcessSubtypeUnknown:
      break;
    case kProcessSubtypeExtensionPersistent:
      RecordProcessHistograms("RendererExtensionPersistentProcess", metrics);
      break;
    case kProcessSubtypeExtensionEvent:
      RecordProcessHistograms("RendererExtensionEventProcess", metrics);
      break;
    case kProcessSubtypeNetworkProcess:
      RecordProcessHistograms("NetworkProcess", metrics);
      break;
  }
}

void ProcessMetricsRecorder::OnAggregatedMetricsSampled(
    const ProcessMonitor::Metrics& metrics) {
  RecordProcessHistograms("Total", metrics);
}

}  // namespace performance_monitor
