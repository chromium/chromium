// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_WORKING_SET_TRIMMER_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_WORKING_SET_TRIMMER_POLICY_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {
namespace policies {

// Empties the working set of processes in which all frames are frozen.
//
// Objective #1: Track working set growth rate.
//   Swap thrashing occurs when a lot of pages are accessed in a short period of
//   time. Swap thrashing can be reduced by reducing the number of pages
//   accessed by processes in which all frames are frozen. To track efforts
//   towards this goal, we empty the working set of processes when all their
//   frames become frozen and record the size of their working set after x
//   minutes.
//   TODO(fdoray): Record the working set size x minutes after emptying it.
//   https://crbug.com/885293
//
// Objective #2: Improve performance.
//   We hypothesize that emptying the working set of a process causes its pages
//   to be compressed and/or written to disk preemptively, which makes more
//   memory available quickly for foreground processes and improves global
//   browser performance.
class WorkingSetTrimmerPolicy : public GraphOwned,
                                public ProcessNode::ObserverDefaultImpl {
 public:
  WorkingSetTrimmerPolicy();
  ~WorkingSetTrimmerPolicy() override;

  // CreatePolicyForPlatform will create a working set trimmer policy for a
  // specific platform which should be owned by the graph, you should always
  // check PlatformSupportsWorkingSetTrim() before creating a policy to do so as
  // it would result in unnecessary book-keeping.
  static std::unique_ptr<WorkingSetTrimmerPolicy> CreatePolicyForPlatform();

  // Returns true if running on a platform that supports working set trimming.
  static bool PlatformSupportsWorkingSetTrim();

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver implementation:
  void OnAllFramesInProcessFrozen(const ProcessNode* process_node) override;

 protected:
  // (Un)registers the various node observer flavors of this object with the
  // graph. These are invoked by OnPassedToGraph and OnTakenFromGraph, but
  // hoisted to their own functions for testing.
  void RegisterObservers(Graph* graph);
  void UnregisterObservers(Graph* graph);

  // Returns the time in which this process was last trimmed.
  base::TimeTicks GetLastTrimTime(const ProcessNode* process_node);

  // Sets the last trim time to TimeTicks::Now().
  void SetLastTrimTimeNow(const ProcessNode* process_node);

  // TrimWorkingSet will trim a ProcessNode's working set, it will return true
  // on success. This is virtual for testing.
  virtual bool TrimWorkingSet(const ProcessNode* process_node);

 private:
  friend class WorkingSetTrimmerPolicyTest;

  // A helper method which sets the last trim time to the specified time.
  void SetLastTrimTime(const ProcessNode* process_node, base::TimeTicks time);

  DISALLOW_COPY_AND_ASSIGN(WorkingSetTrimmerPolicy);
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_POLICIES_WORKING_SET_TRIMMER_POLICY_H_
