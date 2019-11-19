// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_WORKING_SET_TRIMMER_POLICY_CHROMEOS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_WORKING_SET_TRIMMER_POLICY_CHROMEOS_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/policies/policy_features.h"
#include "chrome/browser/performance_manager/graph/policies/working_set_trimmer_policy.h"

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

 protected:
  friend class WorkingSetTrimmerPolicyChromeOSTest;

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  void TrimNodesOnGraph();

  bool trim_on_freeze_enabled_ = false;
  bool trim_on_memory_pressure_enabled_ = false;

  features::TrimOnMemoryPressureParams trim_on_memory_pressure_params_;

  // Keeps track of the last time we walked the graph looking for processes
  // to trim.
  base::TimeTicks last_graph_walk_;

  base::Optional<base::MemoryPressureListener> memory_pressure_listener_;

 private:
  Graph* graph_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WorkingSetTrimmerPolicyChromeOS);
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_WORKING_SET_TRIMMER_POLICY_CHROMEOS_H_
