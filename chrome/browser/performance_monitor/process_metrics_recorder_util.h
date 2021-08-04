// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_RECORDER_UTIL_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_RECORDER_UTIL_H_

#include <vector>

#include "build/build_config.h"
#include "chrome/browser/performance_monitor/process_monitor.h"

namespace performance_monitor {

// Use to record various CPU related histograms based on data from |metrics|
// suffixed with |histogram_suffix|.
void RecordProcessHistograms(const char* histogram_suffix,
                             const ProcessMonitor::Metrics& metrics);

#if defined(OS_MAC)
void RecordCoalitionData(const ProcessMonitor::Metrics& metrics,
                         const std::vector<const char*>& suffixes);
#endif

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_RECORDER_UTIL_H_
