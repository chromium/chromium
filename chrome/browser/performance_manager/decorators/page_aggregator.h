// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_H_

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"

namespace performance_manager {

// Computes page level properties. The current properties tracked by this
// aggregator are:
//   - The usage of WebLocks in one of the page's frames.
//   - The usage of IndexedDB locks in one of the page's frames.
//   - The form interaction bit: This indicates if a form contained in one of
//     the page's frames has been interacted with.
class PageAggregator : public FrameNode::ObserverDefaultImpl,
                       public GraphOwnedDefaultImpl,
                       public NodeDataDescriberDefaultImpl {
 public:
  PageAggregator();

  PageAggregator(const PageAggregator&) = delete;
  PageAggregator& operator=(const PageAggregator&) = delete;

  ~PageAggregator() override;

 private:
  class Data;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnIsCurrentChanged(const FrameNode* frame_node) override;
  void OnFrameIsHoldingWebLockChanged(const FrameNode* frame_node) override;
  void OnFrameIsHoldingIndexedDBLockChanged(
      const FrameNode* frame_node) override;
  void OnHadFormInteractionChanged(const FrameNode* frame_node) override;
  void OnHadUserEditsChanged(const FrameNode* frame_node) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber implementation:
  base::Value DescribePageNodeData(const PageNode* node) const override;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_AGGREGATOR_H_
