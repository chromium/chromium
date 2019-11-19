// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_HISTORY_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_HISTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/process_type.h"

namespace base {
class ProcessMetrics;
}

namespace performance_monitor {
enum ProcessSubtypes {
  kProcessSubtypeUnknown,
  kProcessSubtypePPAPIFlash,
  kProcessSubtypeExtensionPersistent,
  kProcessSubtypeExtensionEvent
};

struct ProcessMetricsMetadata {
  base::ProcessHandle handle;
  int process_type;
  ProcessSubtypes process_subtype;

  ProcessMetricsMetadata()
      : handle(base::kNullProcessHandle),
        process_type(content::PROCESS_TYPE_UNKNOWN),
        process_subtype(kProcessSubtypeUnknown) {}
};

class ProcessMetricsHistory {
 public:
  ProcessMetricsHistory();
  ProcessMetricsHistory(const ProcessMetricsHistory& other) = delete;
  ~ProcessMetricsHistory();

  // Configure this to monitor a specific process.
  void Initialize(const ProcessMetricsMetadata& process_data,
                  int initial_update_sequence);

  // Gather metrics for the process and accumulate with past data.
  void SampleMetrics();

  // Triggers any UMA histograms or background traces if cpu_usage is excessive.
  void RunPerformanceTriggers();

  // Used to mark when this object was last updated, so we can cull
  // dead ones.
  void set_last_update_sequence(int new_update_sequence) {
    last_update_sequence_ = new_update_sequence;
  }

  int last_update_sequence() const { return last_update_sequence_; }

 private:
  // May not be fully populated. e.g. no |id| and no |name| for browser and
  // renderer processes.
  ProcessMetricsMetadata process_data_;
  std::unique_ptr<base::ProcessMetrics> process_metrics_;
  int last_update_sequence_ = 0;

  double cpu_usage_ = 0.0;
#if defined(OS_WIN)
  uint64_t disk_usage_ = 0;
#endif

#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_AIX)
  int idle_wakeups_ = 0;
#endif
#if defined(OS_MACOSX)
  int package_idle_wakeups_ = 0;
  double energy_impact_ = 0.0;
#endif

  DISALLOW_ASSIGN(ProcessMetricsHistory);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_HISTORY_H_
