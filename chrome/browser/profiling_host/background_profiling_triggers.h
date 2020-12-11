// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILING_HOST_BACKGROUND_PROFILING_TRIGGERS_H_
#define CHROME_BROWSER_PROFILING_HOST_BACKGROUND_PROFILING_TRIGGERS_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/timer/timer.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"

namespace heap_profiling {

class ProfilingProcessHost;

// BackgroundProfilingTriggers is used on the browser process to trigger the
// collection of memory dumps and upload the results to the slow-reports
// service. BackgroundProfilingTriggers class sets a periodic timer and
// interacts with ProfilingProcessHost to trigger and upload memory dumps.
//
// When started, memory information is collected every hour to check if any
// process is over the trigger threshold. Once a report is uploaded, the
// collection interval is changed to once every 12 hours.
class BackgroundProfilingTriggers {
 public:
  explicit BackgroundProfilingTriggers(ProfilingProcessHost* host);
  virtual ~BackgroundProfilingTriggers();

  // Register a periodic timer calling |PerformMemoryUsageChecks|.
  void StartTimer();

 protected:
  // High water mark for private footprint of each profiled pid at time of
  // upload. Results are stored in |kb|.
  // Exposed to subclasses for testing.
  std::map<base::ProcessId, uint32_t> pmf_at_last_upload_;

 private:
  friend class FakeBackgroundProfilingTriggers;
  FRIEND_TEST_ALL_PREFIXES(BackgroundProfilingTriggersTest,
                           IsAllowedToUpload_Metrics);
  FRIEND_TEST_ALL_PREFIXES(BackgroundProfilingTriggersTest,
                           IsAllowedToUpload_Incognito);

  // Returns true if trace uploads are allowed.
  bool IsAllowedToUpload() const;

  // Returns true if |private_footprint_kb| is large enough to trigger
  // a report for the given |content_process_type|.
  bool IsOverTriggerThreshold(int content_process_type,
                              uint32_t private_footprint_kb);

  // Check the current memory usage and send a slow-report if needed.
  void PerformMemoryUsageChecks();

  // Called when the memory dump is received. Performs
  // checks on memory usage and trigger a memory report with
  // |TriggerMemoryReportForProcess| if needed.
  void OnReceivedMemoryDump(
      std::vector<base::ProcessId> profiled_pids,
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

  // Virtual for testing. Called when a memory report needs to be send.
  virtual void TriggerMemoryReport(std::string trigger_name);

  ProfilingProcessHost* host_;

  // Timer to periodically check memory consumption and upload a slow-report.
  base::RepeatingTimer timer_;

  base::WeakPtrFactory<BackgroundProfilingTriggers> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackgroundProfilingTriggers);
};

}  // namespace heap_profiling

#endif  // CHROME_BROWSER_PROFILING_HOST_BACKGROUND_PROFILING_TRIGGERS_H_
