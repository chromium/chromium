// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MONITOR_H_

#include <memory>

#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace performance_monitor {
class ProcessMetricsRecorder;
class ProcessMonitor;
class SystemMonitor;
}  // namespace performance_monitor

class ChromeBrowserMainExtraPartsPerformanceMonitor
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsPerformanceMonitor();
  ~ChromeBrowserMainExtraPartsPerformanceMonitor() override;

  ChromeBrowserMainExtraPartsPerformanceMonitor(
      const ChromeBrowserMainExtraPartsPerformanceMonitor&) = delete;
  ChromeBrowserMainExtraPartsPerformanceMonitor& operator=(
      const ChromeBrowserMainExtraPartsPerformanceMonitor&) = delete;

  // ChromeBrowserMainExtraParts:
  void PostMainMessageLoopStart() override;
  void PreMainMessageLoopRun() override;

 private:
  // The process monitor instance. Collects metrics about every child processes.
  std::unique_ptr<performance_monitor::ProcessMonitor> process_monitor_;

  // The system monitor instance, used by some subsystems to collect the system
  // metrics they need.
  std::unique_ptr<performance_monitor::SystemMonitor> system_monitor_;

  // Observes the |process_monitor_| and record histograms from the metrics
  // received.
  std::unique_ptr<performance_monitor::ProcessMetricsRecorder>
      process_metrics_recorder_;
};

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MONITOR_H_
