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
  void Initialize(const ProcessMetadata& process_data,
                  int initial_update_sequence);

  // Gather metrics for the process and accumulate with past data.
  ProcessMonitor::Metrics SampleMetrics();

  // Used to mark when this object was last updated, so we can cull
  // dead ones.
  void set_last_update_sequence(int new_update_sequence) {
    last_update_sequence_ = new_update_sequence;
  }

  const ProcessMetadata& metadata() const { return process_data_; }

  int last_update_sequence() const { return last_update_sequence_; }

 private:
  ProcessMetadata process_data_;
  std::unique_ptr<base::ProcessMetrics> process_metrics_;
  int last_update_sequence_ = 0;
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_HISTORY_H_
