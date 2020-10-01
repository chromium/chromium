// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_chromeos.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/process/arc_process.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/arc/arc_util.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "url/gurl.h"

namespace performance_manager {
namespace policies {

WorkingSetTrimmerPolicyChromeOS::WorkingSetTrimmerPolicyChromeOS() {
  trim_on_memory_pressure_ =
      base::FeatureList::IsEnabled(features::kTrimOnMemoryPressure);
  trim_on_freeze_ = base::FeatureList::IsEnabled(features::kTrimOnFreeze);
  trim_arc_on_memory_pressure_ =
      base::FeatureList::IsEnabled(features::kTrimArcOnMemoryPressure);

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
      // TODO(b/165850234): Integrate with ARCVM.
      trim_arc_on_memory_pressure_ = false;
    }
  }
}

WorkingSetTrimmerPolicyChromeOS::~WorkingSetTrimmerPolicyChromeOS() = default;

// On MemoryPressure we will try to trim the working set of some renders if they
// have been backgrounded for some period of time and have not been trimmed for
// at least the backoff period.
void WorkingSetTrimmerPolicyChromeOS::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  // Try not to walk the graph too frequently because we can receive moderate
  // memory pressure notifications every 10s.

  if (trim_on_memory_pressure_) {
    if (base::TimeTicks::Now() - last_graph_walk_ >
        params_.graph_walk_backoff_time) {
      TrimNodesOnGraph();
    }
  }

  if (trim_arc_on_memory_pressure_) {
    if (base::TimeTicks::Now() - last_arc_process_fetch_ >
        params_.arc_process_list_fetch_backoff_time) {
      TrimArcProcesses();
    }
  }
}

void WorkingSetTrimmerPolicyChromeOS::TrimNodesOnGraph() {
  const base::TimeTicks now_ticks = base::TimeTicks::Now();
  for (const PageNode* page_node : graph_->GetAllPageNodes()) {
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

bool WorkingSetTrimmerPolicyChromeOS::TrimArcProcess(base::ProcessId pid) {
  SetArcProcessLastTrimTime(pid, base::TimeTicks::Now());

  static int arc_processes_trimmed = 0;
  UMA_HISTOGRAM_COUNTS_10000("Memory.WorkingSetTrim.ArcProcessTrimCount",
                             ++arc_processes_trimmed);

  auto* trimmer = static_cast<mechanism::WorkingSetTrimmerChromeOS*>(
      mechanism::WorkingSetTrimmer::GetInstance());
  return trimmer->TrimWorkingSet(pid);
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

void WorkingSetTrimmerPolicyChromeOS::TrimArcProcesses() {
  last_arc_process_fetch_ = base::TimeTicks::Now();

  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (!arc_process_service) {
    return;
  }

  if (params_.trim_arc_app_processes) {
    arc_process_service->RequestAppProcessList(base::BindOnce(
        &WorkingSetTrimmerPolicyChromeOS::TrimReceivedArcProcesses,
        weak_ptr_factory_.GetWeakPtr(),
        params_.arc_max_number_processes_per_trim));
  }

  if (params_.trim_arc_system_processes) {
    arc_process_service->RequestSystemProcessList(base::BindOnce(
        &WorkingSetTrimmerPolicyChromeOS::TrimReceivedArcProcesses,
        weak_ptr_factory_.GetWeakPtr(),
        params_.arc_max_number_processes_per_trim));
  }
}

void WorkingSetTrimmerPolicyChromeOS::OnTakenFromGraph(Graph* graph) {
  memory_pressure_listener_.reset();
  graph_ = nullptr;
  WorkingSetTrimmerPolicy::OnTakenFromGraph(graph);
}

void WorkingSetTrimmerPolicyChromeOS::OnAllFramesInProcessFrozen(
    const ProcessNode* process_node) {
  if (trim_on_freeze_) {
    WorkingSetTrimmerPolicy::OnAllFramesInProcessFrozen(process_node);
  }
}

void WorkingSetTrimmerPolicyChromeOS::OnPassedToGraph(Graph* graph) {
  if (trim_on_memory_pressure_ || trim_arc_on_memory_pressure_) {
    // We wait to register the memory pressure listener so we're on the
    // right sequence.
    params_ = features::TrimOnMemoryPressureParams::GetParams();
    memory_pressure_listener_.emplace(
        FROM_HERE,
        base::BindRepeating(&WorkingSetTrimmerPolicyChromeOS::OnMemoryPressure,
                            base::Unretained(this)));
  }

  graph_ = graph;
  WorkingSetTrimmerPolicy::OnPassedToGraph(graph);
}

// static
bool WorkingSetTrimmerPolicyChromeOS::PlatformSupportsWorkingSetTrim() {
  return mechanism::WorkingSetTrimmer::GetInstance()
      ->PlatformSupportsWorkingSetTrim();
}

}  // namespace policies
}  // namespace performance_manager
