// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/working_set_trimmer_policy_chromeos.h"

#include "base/bind.h"
#include "chrome/browser/performance_manager/graph/policies/policy_features.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "url/gurl.h"

namespace performance_manager {
namespace policies {

WorkingSetTrimmerPolicyChromeOS::WorkingSetTrimmerPolicyChromeOS() {
  trim_on_memory_pressure_enabled_ =
      base::FeatureList::IsEnabled(features::kTrimOnMemoryPressure);
  trim_on_freeze_enabled_ =
      base::FeatureList::IsEnabled(features::kTrimOnFreeze);

  if (trim_on_memory_pressure_enabled_) {
    trim_on_memory_pressure_params_ =
        features::TrimOnMemoryPressureParams::GetParams();
  }
}

WorkingSetTrimmerPolicyChromeOS::~WorkingSetTrimmerPolicyChromeOS() = default;

// On MemoryPressure we will try to trim the working set of some renders if they
// have been backgrounded for some period of time and have not been trimmed for
// at least the backoff period.
void WorkingSetTrimmerPolicyChromeOS::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE)
    return;

  // Try not to walk the graph too frequently because we can receive moderate
  // memory pressure notifications every 10s.
  if (base::TimeTicks::Now() - last_graph_walk_ <
      trim_on_memory_pressure_params_.graph_walk_backoff_time) {
    return;
  }

  TrimNodesOnGraph();
}

void WorkingSetTrimmerPolicyChromeOS::TrimNodesOnGraph() {
  const base::TimeTicks now_ticks = base::TimeTicks::Now();
  for (const PageNode* page_node : graph_->GetAllPageNodes()) {
    if (!page_node->IsVisible() &&
        page_node->GetTimeSinceLastVisibilityChange() >
            trim_on_memory_pressure_params_.node_invisible_time) {
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
        if (now_ticks - last_trim >
            trim_on_memory_pressure_params_.node_trim_backoff_time) {
          TrimWorkingSet(process_node);
        }
      }
    }
  }
  last_graph_walk_ = now_ticks;
}

void WorkingSetTrimmerPolicyChromeOS::OnTakenFromGraph(Graph* graph) {
  memory_pressure_listener_.reset();
  graph_ = nullptr;
  WorkingSetTrimmerPolicy::OnTakenFromGraph(graph);
}

void WorkingSetTrimmerPolicyChromeOS::OnAllFramesInProcessFrozen(
    const ProcessNode* process_node) {
  if (trim_on_freeze_enabled_) {
    WorkingSetTrimmerPolicy::OnAllFramesInProcessFrozen(process_node);
  }
}

void WorkingSetTrimmerPolicyChromeOS::OnPassedToGraph(Graph* graph) {
  if (trim_on_memory_pressure_enabled_) {
    // We wait to register the memory pressure listener so we're on the
    // right sequence.
    trim_on_memory_pressure_params_ =
        features::TrimOnMemoryPressureParams::GetParams();
    memory_pressure_listener_.emplace(
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
