// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSVM_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSVM_METRICS_H_

#include <set>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/system/procfs_util.h"

namespace crostini {

// Measure and report crosvm host process CPU and memory usage metrics.
class CrosvmMetrics {
 public:
  CrosvmMetrics();
  ~CrosvmMetrics();

  // Start taking snapshot of crosvm process resource usage.
  void Start();

  using PidStatMap =
      std::unordered_map<pid_t, chromeos::system::SingleProcStat>;

  // Returns Crosvm process resident memory percentage of the total used memory.
  // This is exposed only for testing.
  static int CalculateCrosvmRssPercentage(const PidStatMap& pid_stat_map,
                                          int64_t mem_used,
                                          int64_t page_size);
  // Returns Crosvm process CPU usage percentage of the system CPU usage.
  // This is exposed only for testing.
  static int CalculateCrosvmCpuPercentage(
      const PidStatMap& pid_stat_map,
      const PidStatMap& previous_pid_stat_map,
      int64_t cycle_cpu_time);

 private:
  // Callback to post MetricsCycle() to a blocking thread.
  void MetricsCycleCallback();

  // Measures resource metrics and emit UMA histograms.
  void MetricsCycle();

  // Collect the crosvm process data and system CPU usage data at the start of
  // the 10-minute cycle.
  void CollectCycleStartData();

  // The timer is used to call MetricsCycleCallback() to periodically measure
  // crosvm processes CPU and memory usage and send them as UMA histograms.
  base::RepeatingTimer timer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  bool cycle_start_data_collected_ = false;
  int64_t previous_total_cpu_time_ = 0;
  PidStatMap previous_pid_stat_map_;
  int64_t page_size_;  // In bytes.

  base::FilePath slash_proc_ = base::FilePath("/proc");

  base::WeakPtrFactory<CrosvmMetrics> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrosvmMetrics);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSVM_METRICS_H_
