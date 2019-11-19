// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/system_monitor_metrics_logger.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"

namespace performance_monitor {

using SamplingFrequency = SystemMonitor::SamplingFrequency;
using MetricsRefreshFrequencies =
    SystemMonitor::SystemObserver::MetricRefreshFrequencies;

SystemMonitorMetricsLogger::SystemMonitorMetricsLogger() {
  // These metrics are only available on Windows for now.
#if defined(OS_WIN)
  if (auto* system_monitor = SystemMonitor::Get()) {
    system_monitor->AddOrUpdateObserver(
        this,
        MetricsRefreshFrequencies::Builder()
            .SetFreePhysMemoryMbFrequency(SamplingFrequency::kDefaultFrequency)
            .SetDiskIdleTimePercentFrequency(
                SamplingFrequency::kDefaultFrequency)
            .Build());
  }
#endif
}

void SystemMonitorMetricsLogger::OnFreePhysicalMemoryMbSample(
    int free_phys_memory_mb) {
  UMA_HISTOGRAM_COUNTS_1M("PerformanceMonitor.SystemMonitor.FreePhysMemory",
                          free_phys_memory_mb);
}

void SystemMonitorMetricsLogger::OnDiskIdleTimePercent(
    float disk_idle_time_percent) {
  UMA_HISTOGRAM_COUNTS_100("PerformanceMonitor.SystemMonitor.DiskIdleTime",
                           disk_idle_time_percent * 100);
}

}  // namespace performance_monitor
