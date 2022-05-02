// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/process_metrics_recorder_util.h"

#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"

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
      base::StrCat({"PerformanceMonitor.AverageCPU2.", histogram_suffix}),
      metrics.cpu_usage * kCPUUsageFactor, kCPUUsageHistogramMin,
      kCPUUsageHistogramMax, kCPUUsageHistogramBucketCount);
#if BUILDFLAG(IS_WIN)
  base::UmaHistogramCustomCounts(
      base::StrCat({"PerformanceMonitor.AverageCPU3.", histogram_suffix}),
      metrics.precise_cpu_usage * kCPUUsageFactor, kCPUUsageHistogramMin,
      kCPUUsageHistogramMax, kCPUUsageHistogramBucketCount);
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
  base::UmaHistogramCounts10000(
      base::StrCat({"PerformanceMonitor.IdleWakeups.", histogram_suffix}),
      metrics.idle_wakeups);
#endif
#if BUILDFLAG(IS_MAC)
  base::UmaHistogramCounts1000(
      base::StrCat(
          {"PerformanceMonitor.PackageExitIdleWakeups.", histogram_suffix}),
      metrics.package_idle_wakeups);
  base::UmaHistogramCounts100000(
      base::StrCat({"PerformanceMonitor.EnergyImpact.", histogram_suffix}),
      metrics.energy_impact);
#endif
}
