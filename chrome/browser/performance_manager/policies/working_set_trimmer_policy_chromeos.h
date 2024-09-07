// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_CHROMEOS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_CHROMEOS_H_

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/process/arc_process_service.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/browser/browser_thread.h"

namespace arc {
class ArcProcess;
}

namespace performance_manager {
namespace policies {

class WorkingSetTrimmerPolicyChromeOSTest;

// ChromeOS specific WorkingSetTrimmerPolicy which uses the default policy on
// all frames frozen, additionally it will add working set trim under memory
// pressure.
class WorkingSetTrimmerPolicyChromeOS : public WorkingSetTrimmerPolicy,
                                        chromeos::PowerManagerClient::Observer {
 public:
  // A delegate interface for checking ARCVM status. This interface allows us 1)
  // to test WorkingSetTrimmerPolicyChromeOS more easily, and 2) to have all the
  // complicated logic that is very ARCVM specific in a separate class.
  class ArcVmDelegate {
   public:
    virtual ~ArcVmDelegate() = default;

    // Returns ReclaimType other than kReclaimNone when ARCVM has been idle for
    // more than |arcvm_inactivity_time| and therefore is safe to reclaim its
    // memory. The function is called only on the UI thread.
    // If |trim_once_type_after_arcvm_boot| is not kReclaimNone, the function
    // returns |trim_once_type_after_arcvm_boot| when the function is called for
    // the first time after ARCVM boot - in which case it will set the
    // value of |is_first_trim_post_boot| output parameter to true.
    // Use NULL for |is_first_trim_post_boot| if you don't care about
    // whether it is the first call post-boot or not.
    virtual mechanism::ArcVmReclaimType IsEligibleForReclaim(
        const base::TimeDelta& arcvm_inactivity_time,
        mechanism::ArcVmReclaimType trim_once_type_after_arcvm_boot,
        bool* is_first_trim_post_boot) = 0;
  };

  WorkingSetTrimmerPolicyChromeOS(const WorkingSetTrimmerPolicyChromeOS&) =
      delete;
  WorkingSetTrimmerPolicyChromeOS& operator=(
      const WorkingSetTrimmerPolicyChromeOS&) = delete;

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

  // PowerManagerClient::Observer implementations:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta duration) override;

  // We maintain the last time we reclaimed an ARC process, to allow us to not
  // reclaim too frequently, this is configurable.
  base::TimeDelta GetTimeSinceLastArcProcessTrim(base::ProcessId pid) const;
  void SetArcProcessLastTrimTime(base::ProcessId pid, base::TimeTicks time);

  bool trim_on_freeze() const { return trim_on_freeze_; }
  bool trim_arc_on_memory_pressure() const {
    return trim_arc_on_memory_pressure_;
  }
  void set_arcvm_delegate_for_testing(ArcVmDelegate* delegate);

  virtual void TrimReceivedArcProcesses(
      int allowed_to_trim,
      arc::ArcProcessService::OptionalArcProcessList arc_processes);

 protected:
  friend class WorkingSetTrimmerPolicyChromeOSTest;

  // virtual for testing
  virtual void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);
  virtual mechanism::WorkingSetTrimmerChromeOS* GetTrimmer();

  void set_trim_on_freeze(bool enabled) { trim_on_freeze_ = enabled; }

  void set_trim_arc_on_memory_pressure(bool enabled) {
    trim_arc_on_memory_pressure_ = enabled;
  }
  void set_trim_arcvm_on_memory_pressure(bool enabled) {
    trim_arcvm_on_memory_pressure_ = enabled;
  }

  void TrimNodesOnGraph();

  // TrimArcProcesses will walk procfs looking for ARC container processes which
  // can be trimmed. These are virtual for testing.
  virtual void TrimArcProcesses();
  virtual bool IsArcProcessEligibleForReclaim(
      const arc::ArcProcess& arc_process);
  virtual void TrimArcProcess(base::ProcessId pid);

  // TrimArcVmProcesses will ask the delegate if it is safe to reclaim memory
  // from ARCVM, and do that when it is. These are virtual for testing.
  virtual void TrimArcVmProcesses(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // The functions below form a chain of callbacks that carry along
  // a WeakPtr to an instance of WorkingSetTrimmerPolicyChromeOS.
  // That instance can be destroyed at any point in the chain.
  // The following constraints apply:
  // - The WeakPtr is built on the PM thread, so it can only be
  //   checked for validity or dereferenced in the PM thread.
  // - For member functions, there is an automatic pointer check inside
  //   BindOnce at invocation, if the "this" is a WeakPtr.
  // Because of the combination of constraints above, the methods that
  // execute on threads other than the PM thread must be made static,
  // and they must not use the WeakPtr except to pass it down the chain.
  static void TrimArcVmProcessesOnUIThread(
      base::MemoryPressureListener::MemoryPressureLevel level,
      features::TrimOnMemoryPressureParams params,
      base::WeakPtr<WorkingSetTrimmerPolicyChromeOS> ptr);
  virtual void OnTrimArcVmProcesses(mechanism::ArcVmReclaimType reclaim_type,
                                    bool is_first_trim_post_boot,
                                    int pages_per_minute,
                                    int max_pages_per_iteration);
  virtual void OnArcVmTrimStarting();
  static void DoTrimArcVmOnUIThread(
      base::WeakPtr<WorkingSetTrimmerPolicyChromeOS> ptr,
      mechanism::WorkingSetTrimmerChromeOS* trimmer,
      mechanism::ArcVmReclaimType reclaim_type,
      int page_limit);
  static void OnTrimArcVmWorkingSetOnUIThread(
      base::WeakPtr<WorkingSetTrimmerPolicyChromeOS> ptr,
      mechanism::ArcVmReclaimType reclaim_type,
      bool success,
      const std::string& failure_reason);
  virtual void OnArcVmTrimEnded(mechanism::ArcVmReclaimType reclaim_type,
                                bool success);

  features::TrimOnMemoryPressureParams params_;

  // Keeps track of the last time we walked the graph looking for processes
  // to trim.
  std::optional<base::TimeTicks> last_graph_walk_;

  // We keep track of the last time we fetched the ARC process list.
  std::optional<base::TimeTicks> last_arc_process_fetch_;

  // We also keep track of the last time we reclaimed memory from ARCVM.
  std::optional<base::TimeTicks> last_arcvm_trim_;
  std::optional<base::TimeTicks> last_arcvm_trim_success_;

  std::optional<base::MemoryPressureListener> memory_pressure_listener_;

 private:
  bool trim_on_freeze_ = false;
  bool trim_arc_on_memory_pressure_ = false;
  bool trim_arcvm_on_memory_pressure_ = false;
  bool disable_trim_while_suspended_ = false;
  // The status of suspend is updated by PowerManagerClient::Observer which runs
  // on the main thread, and is referenced by
  // WorkingSetTrimmerPolicyChromeOS::OnMemoryPressure() which runs on the PM
  // sequence.
  base::Lock mutex_;
  bool is_system_suspended_ GUARDED_BY(mutex_) = false;
  std::optional<base::TimeTicks> last_suspend_done_time_ GUARDED_BY(mutex_);

  // This map contains the last trim time of arc processes.
  std::map<base::ProcessId, base::TimeTicks> arc_processes_last_trim_;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_observation_{this};

  base::WeakPtrFactory<WorkingSetTrimmerPolicyChromeOS> weak_ptr_factory_{this};
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_CHROMEOS_H_
