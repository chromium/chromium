// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_PROCESS_METRICS_RECORDER_H_
#define CHROME_BROWSER_METRICS_POWER_PROCESS_METRICS_RECORDER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/metrics/power/process_monitor.h"

// This class receives the metrics gathered by the ProcessMonitor and records
// some performance-related histograms about the different process types.
class ProcessMetricsRecorder final : public ProcessMonitor::Observer {
 public:
  explicit ProcessMetricsRecorder(ProcessMonitor* process_monitor);
  ~ProcessMetricsRecorder() override;

  ProcessMetricsRecorder(const ProcessMetricsRecorder&) = delete;
  ProcessMetricsRecorder& operator=(const ProcessMetricsRecorder&) = delete;

 private:
  // ProcessMonitor::Observer:
  void OnMetricsSampled(int process_type,
                        ProcessSubtypes process_subtype,
                        const ProcessMonitor::Metrics& metrics) override;

  base::ScopedObservation<ProcessMonitor, ProcessMonitor::Observer>
      process_monitor_observation_{this};
};

#endif  // CHROME_BROWSER_METRICS_POWER_PROCESS_METRICS_RECORDER_H_
