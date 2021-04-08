// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_MONITOR_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/public/common/process_type.h"

namespace performance_monitor {

class ProcessMetricsHistory;

enum ProcessSubtypes {
  kProcessSubtypeUnknown,
  kProcessSubtypeExtensionPersistent,
  kProcessSubtypeExtensionEvent,
  kProcessSubtypeNetworkProcess,
};

struct ProcessMetadata {
  base::ProcessHandle handle = base::kNullProcessHandle;
  int process_type = content::PROCESS_TYPE_UNKNOWN;
  ProcessSubtypes process_subtype = kProcessSubtypeUnknown;
};

// ProcessMonitor is a tool which periodically monitors performance metrics
// of all the Chrome processes for histogram logging and possibly taking action
// upon noticing serious performance degradation.
class ProcessMonitor {
 public:
  struct Metrics {
    // The percentage of time spent executing, across all threads of the
    // process, in the interval since the last time the metric was sampled. This
    // can exceed 100% in multi-thread processes running on multi-core systems.
    double cpu_usage = 0.0;

#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
    // Returns the number of average idle cpu wakeups per second since the last
    // time the metric was sampled.
    int idle_wakeups = 0;
#endif

#if defined(OS_MAC)
    // The number of average "package idle exits" per second since the last
    // time the metric was sampled. See base/process/process_metrics.h for a
    // more detailed explanation.
    int package_idle_wakeups = 0;

    // "Energy Impact" is a synthetic power estimation metric displayed by macOS
    // in Activity Monitor and the battery menu.
    double energy_impact = 0.0;
#endif
  };

  class Observer : public base::CheckedObserver {
   public:
    // Provides the sampled metrics for every Chrome process. This is called
    // once per process at a regular interval.
    virtual void OnMetricsSampled(const ProcessMetadata& process_metadata,
                                  const Metrics& metrics) {}

    // Provides the aggregated sampled metrics from every Chrome process. This
    // is called once at a regular interval regardless of the number of
    // processes.
    virtual void OnAggregatedMetricsSampled(const Metrics& metrics) {}
  };

  // Creates and returns the application-wide ProcessMonitor. Can only be called
  // if no ProcessMonitor instances exists in the current process. The caller
  // owns the created instance. The current process' instance can be retrieved
  // with Get().
  static std::unique_ptr<ProcessMonitor> Create();

  // Returns the application-wide ProcessMonitor if one exists; otherwise
  // returns nullptr.
  static ProcessMonitor* Get();

  virtual ~ProcessMonitor();

  // Start the cycle of metrics gathering.
  void StartGatherCycle();

  // Returns the expected sampling interval according to how it's scheduled.
  // This might vary from the actual interval. Virtual for testing.
  virtual base::TimeDelta GetScheduledSamplingInterval() const;

  // Adds/removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ProcessMonitor();

  base::ObserverList<Observer>& GetObserversForTesting() {
    return observer_list_;
  }

 private:
  // Mark the given process as alive in the current update iteration.
  // This means adding an entry to the map of watched processes if it's not
  // already present.
  void MarkProcessAsAlive(const ProcessMetadata& process_data,
                          int current_update_sequence);

  // Returns the ProcessMetadata for every Chrome processes accessible from the
  // UI thread.
  static std::vector<ProcessMetadata> GatherProcessesOnUIThread();

  // Returns the ProcessMetadata for every Chrome processes accessible from the
  // process thread.
  static std::vector<ProcessMetadata> GatherProcessesOnProcessThread();

  // Gather all the processes from both threads and then invokes GatherMetrics()
  // back on the calling thread.
  void GatherProcesses();

  // Updates the ProcessMetrics map with the current list of processes and
  // gathers metrics from each entry.
  void GatherMetrics(int current_update_sequence,
                     std::vector<ProcessMetadata> ui_thread_processes,
                     std::vector<ProcessMetadata> io_thread_processes);

  // A map of currently running ProcessHandles to ProcessMetrics.
  std::map<base::ProcessHandle, std::unique_ptr<ProcessMetricsHistory>>
      metrics_map_;

  // The timer to signal ProcessMonitor to perform its timed collections.
  base::RepeatingTimer repeating_timer_;

  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<ProcessMonitor> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProcessMonitor);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_MONITOR_H_
