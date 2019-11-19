// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_H_

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager {

// Computes page level properties. The current properties tracked by this
// aggregator are:
//   - The freeze origin trial policy: The aggregation of the freeze
//     origin trial policies of its current frames.
//   - The usage of WebLocks in one of the page's frames.
//   - The usage of IndexedDB locks in one of the page's frames.
class PageAggregator : public FrameNode::ObserverDefaultImpl,
                       public GraphOwnedDefaultImpl {
 public:
  PageAggregator();
  ~PageAggregator() override;

 private:
  class Data;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnIsCurrentChanged(const FrameNode* frame_node) override;
  void OnOriginTrialFreezePolicyChanged(
      const FrameNode* frame_node,
      const InterventionPolicy& previous_value) override;
  void OnFrameIsHoldingWebLockChanged(const FrameNode* frame_node) override;
  void OnFrameIsHoldingIndexedDBLockChanged(
      const FrameNode* frame_node) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  DISALLOW_COPY_AND_ASSIGN(PageAggregator);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_H_
