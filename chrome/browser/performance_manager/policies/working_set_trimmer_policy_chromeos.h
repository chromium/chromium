// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_CHROMEOS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_CHROMEOS_H_

#include <map>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/process/arc_process_service.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy.h"

namespace arc {
class ArcProcess;
}

namespace performance_manager {
namespace policies {

class WorkingSetTrimmerPolicyChromeOSTest;

// ChromeOS specific WorkingSetTrimmerPolicy which uses the default policy on
// all frames frozen, additionally it will add working set trim under memory
// pressure.
class WorkingSetTrimmerPolicyChromeOS : public WorkingSetTrimmerPolicy {
 public:
  ~WorkingSetTrimmerPolicyChromeOS() override;
  WorkingSetTrimmerPolicyChromeOS();

  // Returns true if this platform supports working set trim, in the case of
  // Windows this will check that the appropriate flags are set for working set
  // trim.
  static bool PlatformSupportsWorkingSetTrim();

  // GraphOwned implementation:
  void OnTakenFromGraph(Graph* graph) override;
  void OnPassedToGraph(Graph* graph) override;

  // ProcessNodeObserver implementation:
  void OnAllFramesInProcessFrozen(const ProcessNode* process_node) override;

  // We maintain the last time we reclaimed an ARC process, to allow us to not
  // reclaim too frequently, this is configurable.
  base::TimeDelta GetTimeSinceLastArcProcessTrim(base::ProcessId pid) const;
  void SetArcProcessLastTrimTime(base::ProcessId pid, base::TimeTicks time);

  bool trim_on_freeze() const { return trim_on_freeze_; }
  bool trim_on_memory_pressure() const { return trim_on_memory_pressure_; }
  bool trim_arc_on_memory_pressure() const {
    return trim_arc_on_memory_pressure_;
  }

  virtual void TrimReceivedArcProcesses(
      int allowed_to_trim,
      arc::ArcProcessService::OptionalArcProcessList arc_processes);

 protected:
  friend class WorkingSetTrimmerPolicyChromeOSTest;

  // virtual for testing
  virtual void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  void set_trim_on_freeze(bool enabled) { trim_on_freeze_ = enabled; }
  void set_trim_on_memory_pressure(bool enabled) {
    trim_on_memory_pressure_ = enabled;
  }
  void set_trim_arc_on_memory_pressure(bool enabled) {
    trim_arc_on_memory_pressure_ = enabled;
  }

  void TrimNodesOnGraph();

  // TrimArcProcesses will walk procfs looking for ARC container processes which
  // can be trimmed. These are virtual for testing.
  virtual void TrimArcProcesses();
  virtual bool IsArcProcessEligibleForReclaim(
      const arc::ArcProcess& arc_process);
  virtual bool TrimArcProcess(base::ProcessId pid);

  features::TrimOnMemoryPressureParams params_;

  // Keeps track of the last time we walked the graph looking for processes
  // to trim.
  base::TimeTicks last_graph_walk_;

  // We keep track of the last time we fetched the ARC process list.
  base::TimeTicks last_arc_process_fetch_;

  base::Optional<base::MemoryPressureListener> memory_pressure_listener_;

 private:
  Graph* graph_ = nullptr;

  bool trim_on_freeze_ = false;
  bool trim_on_memory_pressure_ = false;
  bool trim_arc_on_memory_pressure_ = false;

  // This map contains the last trim time of arc processes.
  std::map<base::ProcessId, base::TimeTicks> arc_processes_last_trim_;

  base::WeakPtrFactory<WorkingSetTrimmerPolicyChromeOS> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WorkingSetTrimmerPolicyChromeOS);
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_CHROMEOS_H_
