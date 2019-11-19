// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PROCESS_PRIORITY_AGGREGATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PROCESS_PRIORITY_AGGREGATOR_H_

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

class ProcessNodeImpl;

// The ProcessPriorityAggregator is responsible for calculating a process
// priority as an aggregate of the priorities of all executions contexts (frames
// and workers) it hosts. A process will inherit the priority of the highest
// priority context that it hosts.
class ProcessPriorityAggregator : public FrameNode::ObserverDefaultImpl,
                                  public GraphOwnedDefaultImpl,
                                  public ProcessNode::ObserverDefaultImpl {
 public:
  class Data;

  ProcessPriorityAggregator();
  ~ProcessPriorityAggregator() override;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnPriorityAndReasonChanged(
      const FrameNode* frame_node,
      const PriorityAndReason& previous_value) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver implementation:
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessPriorityAggregator);
};

// This struct is attached to process nodes using NodeAttachedData.
class ProcessPriorityAggregator::Data {
 public:
  static constexpr size_t kExpectedSize =
#if DCHECK_IS_ON()
      12;
#else
      8;
#endif

  // Decrements/increments the appropriate count variable.
  void Decrement(base::TaskPriority priority);
  void Increment(base::TaskPriority priority);

  // Returns true if the various priority counts are all zero.
  bool IsEmpty() const;

  // Calculates the priority that should be upstreamed given the counts.
  base::TaskPriority GetPriority() const;

  uint32_t user_visible_count_for_testing() const {
    return user_visible_count_;
  }

  uint32_t user_blocking_count_for_testing() const {
    return user_blocking_count_;
  }

  static Data* GetForTesting(ProcessNodeImpl* process_node);

 private:
  // The number of frames at the given priority levels. The lowest priority
  // level isn't explicitly tracked as that's the default level.
#if DCHECK_IS_ON()
  // This is only tracked in DCHECK builds as a sanity check. It's not needed
  // because all processes will default to the lowest priority in the absence of
  // higher priority votes.
  uint32_t lowest_count_ = 0;
#endif
  uint32_t user_visible_count_ = 0;
  uint32_t user_blocking_count_ = 0;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PROCESS_PRIORITY_AGGREGATOR_H_
