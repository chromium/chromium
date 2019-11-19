// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_FROZEN_FRAME_AGGREGATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_FROZEN_FRAME_AGGREGATOR_H_

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

class FrameNodeImpl;
class PageNodeImpl;
class ProcessNodeImpl;

// The FrozenFrameAggregator is responsible for tracking frame frozen states,
// and aggregating this property to the page and process nodes. As a GraphOwned
// object it takes care of registering itself as an observer when added to the
// graph.
class FrozenFrameAggregator : public FrameNode::ObserverDefaultImpl,
                              public GraphOwnedDefaultImpl,
                              public PageNode::ObserverDefaultImpl,
                              public ProcessNode::ObserverDefaultImpl {
 public:
  struct Data;

  // TODO(chrisha): Check that the graph is empty when this observer is added!
  // https://www.crbug.com/952891
  FrozenFrameAggregator();
  ~FrozenFrameAggregator() override;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnIsCurrentChanged(const FrameNode* frame_node) override;
  void OnFrameLifecycleStateChanged(const FrameNode* frame_node) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override;

  // ProcessNodeObserver implementation:
  void OnProcessNodeAdded(const ProcessNode* process_node) override;

 protected:
  friend class FrozenFrameAggregatorTest;

  // (Un)registers the various node observer flavors of this object with the
  // graph. These are invoked by OnPassedIntoGraph and OnTakenFromGraph, but
  // hoisted to their own functions for testing.
  void RegisterObservers(Graph* graph);
  void UnregisterObservers(Graph* graph);

  // Used to update counts when adding or removing a |frame_node|. A |delta| of
  // -1 indicates a removal, while +1 indicates adding.
  void AddOrRemoveFrame(FrameNodeImpl* frame_node, int32_t delta);

  // Updates the frame counts associated with the given |frame_node|. Takes
  // care of updating page and process state, as well as firing any needed
  // notifications.
  void UpdateFrameCounts(FrameNodeImpl* frame_node,
                         int32_t current_frame_delta,
                         int32_t frozen_frame_delta);

 private:
  DISALLOW_COPY_AND_ASSIGN(FrozenFrameAggregator);
};

// This struct is stored internally on page and process nodes using
// InlineNodeAttachedDataStorage.
struct FrozenFrameAggregator::Data {
  // The number of current frames associated with a given page/process.
  uint32_t current_frame_count = 0;

  // The number of frozen current frames associated with a given page/process.
  // This is always <= |current_frame_count|.
  uint32_t frozen_frame_count = 0;

  static Data* GetForTesting(PageNodeImpl* page_node);
  static Data* GetForTesting(ProcessNodeImpl* process_node);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_FROZEN_FRAME_AGGREGATOR_H_
