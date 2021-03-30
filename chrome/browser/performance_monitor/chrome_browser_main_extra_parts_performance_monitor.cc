// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/chrome_browser_main_extra_parts_performance_monitor.h"

#include "chrome/browser/performance_monitor/process_metrics_recorder.h"
#include "chrome/browser/performance_monitor/process_monitor.h"
#include "chrome/browser/performance_monitor/system_monitor.h"

ChromeBrowserMainExtraPartsPerformanceMonitor::
    ChromeBrowserMainExtraPartsPerformanceMonitor() = default;

ChromeBrowserMainExtraPartsPerformanceMonitor::
    ~ChromeBrowserMainExtraPartsPerformanceMonitor() = default;

void ChromeBrowserMainExtraPartsPerformanceMonitor::PostMainMessageLoopStart() {
  process_monitor_ = performance_monitor::ProcessMonitor::Create();
  system_monitor_ = performance_monitor::SystemMonitor::Create();
}

void ChromeBrowserMainExtraPartsPerformanceMonitor::PreMainMessageLoopRun() {
  process_metrics_recorder_ =
      std::make_unique<performance_monitor::ProcessMetricsRecorder>(
          process_monitor_.get());

  process_monitor_->StartGatherCycle();
}
