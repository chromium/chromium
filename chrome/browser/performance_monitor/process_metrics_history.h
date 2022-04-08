// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_HISTORY_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_HISTORY_H_

#include <memory>

#include "chrome/browser/performance_monitor/process_monitor.h"

namespace base {
class ProcessMetrics;
}

namespace performance_monitor {

class ProcessMetricsHistory {
 public:
  ProcessMetricsHistory();

  ProcessMetricsHistory(const ProcessMetricsHistory& other) = delete;
  ProcessMetricsHistory& operator=(const ProcessMetricsHistory& other) = delete;

  ~ProcessMetricsHistory();

  // Configure this to monitor a specific process.
  void Initialize(const ProcessMetadata& process_data);

  // Gather metrics for the process and accumulate with past data.
  ProcessMonitor::Metrics SampleMetrics();

  const ProcessMetadata& metadata() const { return process_data_; }

 private:
  ProcessMetadata process_data_;
  std::unique_ptr<base::ProcessMetrics> process_metrics_;
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_HISTORY_H_
