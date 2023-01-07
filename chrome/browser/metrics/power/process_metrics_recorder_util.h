// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_PROCESS_METRICS_RECORDER_UTIL_H_
#define CHROME_BROWSER_METRICS_POWER_PROCESS_METRICS_RECORDER_UTIL_H_

#include "chrome/browser/metrics/power/process_monitor.h"

// Use to record various CPU related histograms based on data from |metrics|
// suffixed with |histogram_suffix|.
void RecordProcessHistograms(const char* histogram_suffix,
                             const ProcessMonitor::Metrics& metrics);

#endif  // CHROME_BROWSER_METRICS_POWER_PROCESS_METRICS_RECORDER_UTIL_H_
