// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PROCESS_MEMORY_METRICS_EMITTER_H_
#define CHROME_BROWSER_METRICS_PROCESS_MEMORY_METRICS_EMITTER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom-data-view.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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

  ProcessMemoryMetricsEmitter(const ProcessMemoryMetricsEmitter&) = delete;
  ProcessMemoryMetricsEmitter& operator=(const ProcessMemoryMetricsEmitter&) =
      delete;

  // This must be called on the main thread of the browser process.
  void FetchAndEmitProcessMemoryMetrics();

 protected:
  virtual ~ProcessMemoryMetricsEmitter();

  // Virtual for testing. Callback invoked when memory_instrumentation service
  // is finished taking a memory dump. `process_infos` is info from the
  // performance_manager for each process.
  virtual void ReceivedMemoryDump(
      absl::flat_hash_map<base::ProcessId, ProcessInfo> process_infos,
      memory_instrumentation::mojom::RequestOutcome outcome,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

  // Virtual for testing. Gets info about each process from performance_manager.
  virtual absl::flat_hash_map<base::ProcessId, ProcessInfo>
  GetProcessToPageInfoMap(performance_manager::Graph* graph);

  // Virtual for testing.
  virtual ukm::UkmRecorder* GetUkmRecorder();

  // Virtual for testing. Returns the number of extensions in the given process.
  // It excludes hosted apps extensions.
  virtual int GetNumberOfExtensions(base::ProcessId pid);

  // Virtual for testing. Returns the process uptime of the given process. Does
  // not return a value when the process startup time is not set.
  virtual std::optional<base::TimeDelta> GetProcessUptime(
      base::TimeTicks now,
      const ProcessInfo* process_info);

 private:
  friend class base::RefCountedThreadSafe<ProcessMemoryMetricsEmitter>;

  // Specify this pid_scope_ to only record the memory metrics of the specific
  // process.
  base::ProcessId pid_scope_ = base::kNullProcessId;

  SEQUENCE_CHECKER(sequence_checker_);
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
  base::TimeTicks launch_time;
};

#endif  // CHROME_BROWSER_METRICS_PROCESS_MEMORY_METRICS_EMITTER_H_
