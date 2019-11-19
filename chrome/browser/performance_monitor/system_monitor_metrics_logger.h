// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_METRICS_LOGGER_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_METRICS_LOGGER_H_

#include "base/macros.h"
#include "chrome/browser/performance_monitor/system_monitor.h"

namespace performance_monitor {

// Logger responsible of logging the metrics collected by SystemMonitor to
// UMA. This class register itself as an observer of SystemMonitor on
// construction.
class SystemMonitorMetricsLogger : public SystemMonitor::SystemObserver {
 public:
  SystemMonitorMetricsLogger();
  ~SystemMonitorMetricsLogger() override = default;

 private:
  // SystemMonitor::SystemObserver.
  void OnFreePhysicalMemoryMbSample(int free_phys_memory_mb) override;
  void OnDiskIdleTimePercent(float disk_idle_time_percent) override;

  DISALLOW_ASSIGN(SystemMonitorMetricsLogger);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_METRICS_LOGGER_H_
