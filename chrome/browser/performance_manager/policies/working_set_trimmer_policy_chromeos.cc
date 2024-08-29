// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_chromeos.h"

#include "ash/components/arc/arc_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/process/arc_process.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_arcvm.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace performance_manager {
namespace policies {

namespace {
// TODO(crbug.com/40755583): Remove the global static variable and make it
// GraphOwned once performance_manager code is migrated to UI thread.
WorkingSetTrimmerPolicyChromeOS::ArcVmDelegate* g_arcvm_delegate_for_testing =
    nullptr;

enum ArcProcessType { kApp, kSystem };
void GetArcProcessListOnUIThread(
    ArcProcessType type,
    base::WeakPtr<
        performance_manager::policies::WorkingSetTrimmerPolicyChromeOS> ptr,
    int processes_per_trim) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (!arc_process_service) {
    return;
  }

  // Now we need to bounce back to the PM sequence so we can do stuff with the
  // process list.
  auto callback = base::BindOnce(
      [](decltype(ptr) ptr, decltype(processes_per_trim) processes_per_trim,
         arc::ArcProcessService::OptionalArcProcessList opt_proc_list) {
        PerformanceManager::CallOnGraph(
            FROM_HERE,
            base::BindOnce(
                &WorkingSetTrimmerPolicyChromeOS::TrimReceivedArcProcesses, ptr,
                processes_per_trim, std::move(opt_proc_list)));
      },
      ptr, processes_per_trim);

  if (type == kApp) {
    arc_process_service->RequestAppProcessList(std::move(callback));
  } else if (type == kSystem) {
    arc_process_service->RequestSystemProcessList(std::move(callback));
  }
}

}  // namespace

WorkingSetTrimmerPolicyChromeOS::WorkingSetTrimmerPolicyChromeOS() {
  trim_on_freeze_ = base::FeatureList::IsEnabled(features::kTrimOnFreeze);
  trim_arc_on_memory_pressure_ =
      base::FeatureList::IsEnabled(features::kTrimArcOnMemoryPressure);
  trim_arcvm_on_memory_pressure_ =
      base::FeatureList::IsEnabled(features::kTrimArcVmOnMemoryPressure);
  disable_trim_while_suspended_ =
      base::FeatureList::IsEnabled(features::kDisableTrimmingWhileSuspended);

  params_ = features::TrimOnMemoryPressureParams::GetParams();

  if (trim_arc_on_memory_pressure_) {
    // Validate ARC parameters.
    if (!params_.trim_arc_app_processes && !params_.trim_arc_system_processes) {
      LOG(ERROR)
          << "Misconfiguration ARC trimming on memory pressure is enabled "
             "but both app and system process trimming are disabled.";
      trim_arc_on_memory_pressure_ = false;
    } else if (!arc::IsArcAvailable()) {
      DLOG(ERROR) << "ARC is not available";
      trim_arc_on_memory_pressure_ = false;
    } else if (arc::IsArcVmEnabled()) {
      // ARCVM is handled separately.
      trim_arc_on_memory_pressure_ = false;
    }
  }

  if (trim_arcvm_on_memory_pressure_) {
    if (!arc::IsArcAvailable() || !arc::IsArcVmEnabled()) {
      DLOG(ERROR) << "ARCVM is not available";
      trim_arcvm_on_memory_pressure_ = false;
    }
  }

  if (disable_trim_while_suspended_) {
    power_manager_observation_.Observe(chromeos::PowerManagerClient::Get());
  }
}

WorkingSetTrimmerPolicyChromeOS::~WorkingSetTrimmerPolicyChromeOS() = default;

// On MemoryPressure we will try to trim the working set of some renders if they
// have been backgrounded for some period of time and have not been trimmed for
// at least the backoff period.
void WorkingSetTrimmerPolicyChromeOS::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  bool skip_trimming_due_to_suspend = false;
  if (disable_trim_while_suspended_) {
    base::TimeTicks now = base::TimeTicks::Now();
    base::AutoLock lock(mutex_);
    skip_trimming_due_to_suspend =
        is_system_suspended_ ||
        (last_suspend_done_time_ &&
         now - *last_suspend_done_time_ < params_.suspend_backoff_time);
  }
  // We define idle as the last visible time be greater than some threshold.
  // Since the monotonic clock can keep on ticking during suspend (by dark
  // resume) when we resume it can look like the tab has not been used in some
  // huge amount of time. In reality, the user hasn't been doing anything.
  // Waiting for kSuspendBackoffTimeSec after resuming ensures that enough time
  // has elapsed so that inappropriately added time from dark resume can no
  // longer affect whether or not a tab has been invisible for long enough to be
  // eligible for trimming.
  if (skip_trimming_due_to_suspend) {
    return;
  }
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  // Try not to walk the graph too frequently because we can receive moderate
  // memory pressure notifications every 10s.

  if (!last_graph_walk_ || (base::TimeTicks::Now() - *last_graph_walk_ >
                            params_.graph_walk_backoff_time)) {
    TrimNodesOnGraph();
  }

  if (trim_arc_on_memory_pressure_) {
    if (!last_arc_process_fetch_ ||
        (base::TimeTicks::Now() - *last_arc_process_fetch_ >
         params_.arc_process_list_fetch_backoff_time)) {
      TrimArcProcesses();
    }
  }

  if (trim_arcvm_on_memory_pressure_) {
    if (!last_arcvm_trim_ || (base::TimeTicks::Now() - *last_arcvm_trim_ >
                              params_.arcvm_trim_backoff_time)) {
      TrimArcVmProcesses(level);
    }
  }
}

void WorkingSetTrimmerPolicyChromeOS::set_arcvm_delegate_for_testing(
    ArcVmDelegate* delegate) {
  DCHECK(!g_arcvm_delegate_for_testing || !delegate);
  g_arcvm_delegate_for_testing = delegate;
}

void WorkingSetTrimmerPolicyChromeOS::TrimNodesOnGraph() {
  const base::TimeTicks now_ticks = base::TimeTicks::Now();
  for (const PageNode* page_node : GetOwningGraph()->GetAllPageNodes()) {
    if (!page_node->IsVisible() &&
        page_node->GetTimeSinceLastVisibilityChange() >
            params_.node_invisible_time) {
      // Get the process node and if it has not been
      // trimmed within the backoff period, we will do that
      // now.

      // Check that we have a main frame.
      const FrameNode* frame_node = page_node->GetMainFrameNode();
      if (!frame_node) {
        continue;
      }

      const ProcessNode* process_node = frame_node->GetProcessNode();
      if (process_node && process_node->GetProcess().IsValid()) {
        base::TimeTicks last_trim = GetLastTrimTime(process_node);
        if (now_ticks - last_trim > params_.node_trim_backoff_time) {
          TrimWorkingSet(process_node);
        }
      }
    }
  }
  last_graph_walk_ = now_ticks;
}

base::TimeDelta WorkingSetTrimmerPolicyChromeOS::GetTimeSinceLastArcProcessTrim(
    base::ProcessId pid) const {
  base::TimeDelta delta(base::TimeDelta::Max());
  const auto it = arc_processes_last_trim_.find(pid);
  if (it != arc_processes_last_trim_.end()) {
    delta = base::TimeTicks::Now() - it->second;
  }
  return delta;
}

void WorkingSetTrimmerPolicyChromeOS::SetArcProcessLastTrimTime(
    base::ProcessId pid,
    base::TimeTicks time) {
  arc_processes_last_trim_[pid] = time;
}

bool WorkingSetTrimmerPolicyChromeOS::IsArcProcessEligibleForReclaim(
    const arc::ArcProcess& arc_process) {
  // Focused apps will never be reclaimed.
  if (arc_process.is_focused()) {
    return false;
  }

  if (!params_.trim_arc_aggressive) {
    // By default (non-aggressive) we will only trim unimportant apps
    // non-background protected apps.
    if (arc_process.IsImportant() || arc_process.IsBackgroundProtected()) {
      return false;
    }
  }

  // Next we need to check if it's been reclaimed too recently, if configured.
  if (params_.arc_process_trim_backoff_time != base::TimeDelta::Min()) {
    if (GetTimeSinceLastArcProcessTrim(arc_process.pid()) <
        params_.arc_process_trim_backoff_time) {
      return false;
    }
  }

  // Finally we check if the last activity time was longer than the configured
  // threshold, if configured.
  if (params_.arc_process_inactivity_time != base::TimeDelta::Min()) {
    // Are we within the threshold?
    if ((base::TimeTicks::Now() -
         base::TimeTicks::FromUptimeMillis(arc_process.last_activity_time())) <
        params_.arc_process_inactivity_time) {
      return false;
    }
  }

  return true;
}

mechanism::WorkingSetTrimmerChromeOS*
WorkingSetTrimmerPolicyChromeOS::GetTrimmer() {
  return static_cast<mechanism::WorkingSetTrimmerChromeOS*>(
      mechanism::WorkingSetTrimmer::GetInstance());
}

void WorkingSetTrimmerPolicyChromeOS::TrimArcProcess(base::ProcessId pid) {
  SetArcProcessLastTrimTime(pid, base::TimeTicks::Now());

  GetTrimmer()->TrimWorkingSet(pid);
}

void WorkingSetTrimmerPolicyChromeOS::TrimReceivedArcProcesses(
    int allowed_to_trim,
    arc::ArcProcessService::OptionalArcProcessList arc_processes) {
  if (!arc_processes.has_value()) {
    return;
  }

  // Because ARC may return the same list in order, we shuffle them each time in
  // case we have a small number of arc_available_processes_to_trim_ we don't
  // want to retrim the same ones every time.
  auto& procs = arc_processes.value();
  base::RandomShuffle(procs.begin(), procs.end());

  for (const auto& proc : procs) {
    // If we've already reclaimed too much, we bail.
    if (!allowed_to_trim) {
      break;
    }

    if (IsArcProcessEligibleForReclaim(proc)) {
      TrimArcProcess(proc.pid());

      if (allowed_to_trim > 0) {
        allowed_to_trim--;
      }
    }
  }
}

// TrimArcProcesses will be called on the PM Sequence, we'll need to bounce to
// the UI thread to get the Arc process list and we'll bounce back to the PM
// sequence to do the actual trimming and book keeping.
void WorkingSetTrimmerPolicyChromeOS::TrimArcProcesses() {
  last_arc_process_fetch_ = base::TimeTicks::Now();

  // The fetching of the ARC process list must happen on the UI thread.
  if (params_.trim_arc_system_processes) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&GetArcProcessListOnUIThread, ArcProcessType::kSystem,
                       weak_ptr_factory_.GetWeakPtr(),
                       params_.arc_max_number_processes_per_trim));
  }

  if (params_.trim_arc_app_processes) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&GetArcProcessListOnUIThread, ArcProcessType::kApp,
                       weak_ptr_factory_.GetWeakPtr(),
                       params_.arc_max_number_processes_per_trim));
  }
}

void WorkingSetTrimmerPolicyChromeOS::TrimArcVmProcesses(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  DCHECK_NE(level, base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  // TODO(crbug.com/40755583): Remove the PostTask once performance_manager code
  // is migrated to UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&TrimArcVmProcessesOnUIThread, level, params_,
                                weak_ptr_factory_.GetWeakPtr()));
}

// static
void WorkingSetTrimmerPolicyChromeOS::TrimArcVmProcessesOnUIThread(
    base::MemoryPressureListener::MemoryPressureLevel level,
    features::TrimOnMemoryPressureParams params,
    base::WeakPtr<WorkingSetTrimmerPolicyChromeOS> ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/40755583): Let the policy own WorkingSetTrimmerPolicyArcVm
  // instance once performance_manager code is migrated to UI thread.
  auto* arcvm_delegate = g_arcvm_delegate_for_testing
                             ? g_arcvm_delegate_for_testing
                             : WorkingSetTrimmerPolicyArcVm::Get();

  const bool force_reclaim =
      params.trim_arcvm_on_critical_pressure &&
      (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  const mechanism::ArcVmReclaimType trim_once_type_after_arcvm_boot =
      params.trim_arcvm_on_first_memory_pressure_after_arcvm_boot
          ? mechanism::ArcVmReclaimType::kReclaimGuestPageCaches
          : mechanism::ArcVmReclaimType::kReclaimNone;

  bool is_first_trim_post_boot =
      WorkingSetTrimmerPolicyArcVm::kNotFirstReclaimPostBoot;
  const mechanism::ArcVmReclaimType reclaim_type =
      force_reclaim
          ? mechanism::ArcVmReclaimType::kReclaimAll
          : arcvm_delegate->IsEligibleForReclaim(
                params.arcvm_inactivity_time, trim_once_type_after_arcvm_boot,
                &is_first_trim_post_boot);

  // NOTE: To ease unit test, we invoke OnTrimArcVmProcesses even
  // reclaim_type is kReclaimNone.
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(&WorkingSetTrimmerPolicyChromeOS::OnTrimArcVmProcesses,
                     ptr, reclaim_type, is_first_trim_post_boot,
                     params.trim_arcvm_pages_per_minute,
                     params.trim_arcvm_max_pages_per_iteration));
}

void WorkingSetTrimmerPolicyChromeOS::OnTrimArcVmProcesses(
    mechanism::ArcVmReclaimType reclaim_type,
    bool is_first_trim_post_boot,
    int pages_per_minute,
    int max_pages_per_iteration) {
  // If there's nothing to do, cut it short.
  if (reclaim_type == mechanism::ArcVmReclaimType::kReclaimNone)
    return;

  // Computing the page limit requires touching the "this" pointer,
  // so it must be done in the PM thread.
  // Checking that "this" has not yet been deleted is done by BindOnce()
  // at invocation time.
  int page_limit = arc::ArcSession::kNoPageLimit;
  if (!is_first_trim_post_boot) {
    bool per_minute_limit_applied = false;
    if (pages_per_minute != arc::ArcSession::kNoPageLimit &&
        last_arcvm_trim_success_) {
      auto elapsed_mins =
          (base::TimeTicks::Now() - *last_arcvm_trim_success_).InMinutes();
      if (elapsed_mins > 0) {
        page_limit = elapsed_mins * pages_per_minute;
        per_minute_limit_applied = true;
      }  // else, let the per-iteration limit prevail.
    }

    if (max_pages_per_iteration != arc::ArcSession::kNoPageLimit) {
      // If set, the per-iteration max overrides the per-minute value.
      if (!per_minute_limit_applied || max_pages_per_iteration < page_limit)
        page_limit = max_pages_per_iteration;
    }
  }

  // TODO(crbug.com/40755583): Remove the PostTask once performance_manager code
  // is migrated to UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindRepeating(&DoTrimArcVmOnUIThread,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     GetTrimmer(), reclaim_type, page_limit));
  if (reclaim_type == mechanism::ArcVmReclaimType::kReclaimAll)
    OnArcVmTrimStarting();
}

// static
void WorkingSetTrimmerPolicyChromeOS::DoTrimArcVmOnUIThread(
    base::WeakPtr<WorkingSetTrimmerPolicyChromeOS> ptr,
    mechanism::WorkingSetTrimmerChromeOS* trimmer,
    mechanism::ArcVmReclaimType reclaim_type,
    int page_limit) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  trimmer->TrimArcVmWorkingSet(
      base::BindOnce(&OnTrimArcVmWorkingSetOnUIThread, ptr, reclaim_type),
      reclaim_type, page_limit);
}

// static
void WorkingSetTrimmerPolicyChromeOS::OnTrimArcVmWorkingSetOnUIThread(
    base::WeakPtr<WorkingSetTrimmerPolicyChromeOS> ptr,
    mechanism::ArcVmReclaimType reclaim_type,
    bool success,
    const std::string& failure_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // NOTE: To ease unit test, we invoke OnArcVmTrimEnded even when
  // |reclaim_type| is not kReclaimAll.
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(&WorkingSetTrimmerPolicyChromeOS::OnArcVmTrimEnded, ptr,
                     reclaim_type, success));

  if (success) {
    VLOG(2) << "Reclaimed ARCVM memory";
    return;
  }
  LOG(WARNING) << "Failed to reclaim ARCVM memory: " << failure_reason;
}

void WorkingSetTrimmerPolicyChromeOS::OnArcVmTrimStarting() {
  last_arcvm_trim_ = base::TimeTicks::Now();
}

void WorkingSetTrimmerPolicyChromeOS::OnArcVmTrimEnded(
    mechanism::ArcVmReclaimType reclaim_type,
    bool success) {
  if (reclaim_type != mechanism::ArcVmReclaimType::kReclaimAll)
    return;
  if (success)
    last_arcvm_trim_success_ = base::TimeTicks::Now();
}

void WorkingSetTrimmerPolicyChromeOS::OnTakenFromGraph(Graph* graph) {
  memory_pressure_listener_.reset();
  WorkingSetTrimmerPolicy::OnTakenFromGraph(graph);
}

void WorkingSetTrimmerPolicyChromeOS::OnAllFramesInProcessFrozen(
    const ProcessNode* process_node) {
  if (trim_on_freeze_) {
    WorkingSetTrimmerPolicy::OnAllFramesInProcessFrozen(process_node);
  }
}

void WorkingSetTrimmerPolicyChromeOS::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  base::AutoLock lock(mutex_);
  is_system_suspended_ = true;
}

void WorkingSetTrimmerPolicyChromeOS::SuspendDone(base::TimeDelta duration) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::AutoLock lock(mutex_);
  is_system_suspended_ = false;
  last_suspend_done_time_ = now;
}

void WorkingSetTrimmerPolicyChromeOS::OnPassedToGraph(Graph* graph) {
  // We wait to register the memory pressure listener so we're on the
  // right sequence.
  params_ = features::TrimOnMemoryPressureParams::GetParams();
  memory_pressure_listener_.emplace(
      FROM_HERE,
      base::BindRepeating(&WorkingSetTrimmerPolicyChromeOS::OnMemoryPressure,
                          base::Unretained(this)));

  WorkingSetTrimmerPolicy::OnPassedToGraph(graph);
}

// static
bool WorkingSetTrimmerPolicyChromeOS::PlatformSupportsWorkingSetTrim() {
  return mechanism::WorkingSetTrimmer::GetInstance()
      ->PlatformSupportsWorkingSetTrim();
}

}  // namespace policies
}  // namespace performance_manager
