// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PROCESS_MEMORY_METRICS_EMITTER_H_
#define CHROME_BROWSER_METRICS_PROCESS_MEMORY_METRICS_EMITTER_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"

namespace ukm {
class UkmRecorder;
}

namespace performance_manager {
class Graph;
}

// This class asynchronously fetches memory metrics for each process, and then
// emits UMA metrics from those metrics.
// Each instance is self-owned, and will delete itself once it has finished
// emitting metrics.
// This class is an analog to MetricsMemoryDetails, but uses the resource
// coordinator service to fetch data, rather than doing all the processing
// manually.
class ProcessMemoryMetricsEmitter
    : public base::RefCountedThreadSafe<ProcessMemoryMetricsEmitter> {
 public:
  struct PageInfo;
  struct ProcessInfo;

  // Use this constructor to emit UKM and UMA from all processes, i.e.
  // browser process, gpu process, and all renderers.
  ProcessMemoryMetricsEmitter();
  // Use this constructor to emit UKM from only a specified renderer's.
  explicit ProcessMemoryMetricsEmitter(base::ProcessId pid_scope);

  // This must be called on the main thread of the browser process.
  void FetchAndEmitProcessMemoryMetrics();

  // Public for testing.
  void MarkServiceRequestsInProgress();

 protected:
  virtual ~ProcessMemoryMetricsEmitter();

  // Virtual for testing. Callback invoked when memory_instrumentation service
  // is finished taking a memory dump.
  virtual void ReceivedMemoryDump(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

  // Virtual for testing. Callback invoked when the performance_manager
  // returns info for each process.
  virtual void ReceivedProcessInfos(std::vector<ProcessInfo> process_infos);

  // Virtual for testing.
  virtual ukm::UkmRecorder* GetUkmRecorder();

  // Virtual for testing. Returns the number of extensions in the given process.
  // It excludes hosted apps extensions.
  virtual int GetNumberOfExtensions(base::ProcessId pid);

  // Virtual for testing. Returns the process uptime of the given process. Does
  // not return a value when the process startup time is not set.
  virtual base::Optional<base::TimeDelta> GetProcessUptime(
      const base::Time& now,
      base::ProcessId pid);

 private:
  friend class base::RefCountedThreadSafe<ProcessMemoryMetricsEmitter>;

  // This class sends two asynchronous service requests, whose results need to
  // be collated.
  void CollateResults();

  using GetProcessToPageInfoMapCallback =
      base::OnceCallback<void(std::vector<ProcessInfo>)>;
  static void GetProcessToPageInfoMap(GetProcessToPageInfoMapCallback callback,
                                      performance_manager::Graph* graph);

  // The results of each request are cached. When both requests are finished,
  // the results are collated.
  bool memory_dump_in_progress_ = false;
  std::unique_ptr<memory_instrumentation::GlobalMemoryDump> global_dump_;
  bool get_process_urls_in_progress_ = false;

  // The key is ProcessInfo::pid.
  base::flat_map<base::ProcessId, ProcessInfo> process_infos_;

  // Specify this pid_scope_ to only record the memory metrics of the specific
  // process.
  base::ProcessId pid_scope_ = base::kNullProcessId;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitter);
};

// A |PageInfo| describes some metrics about a particular page with respect to
// a given process.
struct ProcessMemoryMetricsEmitter::PageInfo {
  // Identifier to distinguish which UMK Source this |PageInfo| corresponds to.
  ukm::SourceId ukm_source_id;
  // Identifier to distinguish which tab this |PageInfo| corresponds to.
  uint64_t tab_id;
  // True iff the process for this |PageInfo| hosts the main frame of the page.
  bool hosts_main_frame;
  bool is_visible;
  base::TimeDelta time_since_last_navigation;
  base::TimeDelta time_since_last_visibility_change;
};

struct ProcessMemoryMetricsEmitter::ProcessInfo {
  ProcessInfo();
  ProcessInfo(ProcessInfo&& other);
  ~ProcessInfo();
  ProcessInfo& operator=(const ProcessInfo& other);

  base::ProcessId pid;
  std::vector<PageInfo> page_infos;
  base::Time launch_time;
};

#endif  // CHROME_BROWSER_METRICS_PROCESS_MEMORY_METRICS_EMITTER_H_
