// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_SIDE_PANEL_LOADING_VOTER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_SIDE_PANEL_LOADING_VOTER_H_

#include "base/containers/flat_set.h"
#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager::execution_context_priority {

// This voter is responsible for making sure that a Side Panel loads at a high
// priority, even while it is not visible.
//
// Note that an external call to `MarkAsSidePanel` is necessary to tag pages
// that are associated with a Side Panel.
class SidePanelLoadingVoter : public GraphRegisteredImpl<SidePanelLoadingVoter>,
                              public PriorityVoter,
                              public PageNodeObserver,
                              public FrameNodeObserver {
 public:
  static const char kSidePanelLoadingReason[];

  SidePanelLoadingVoter();
  ~SidePanelLoadingVoter() override;

  // This marks `page_node` as a Side Panel contents, so that this class knows
  // to increase its priority during its initial load.
  void MarkAsSidePanel(const PageNode* page_node);

  // Voter:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override;
  void TearDownOnGraph(Graph* graph) override;

  // PageNodeObserver:
  void OnPageNodeAdded(const PageNode* page_node) override {}
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnMainFrameDocumentChanged(const PageNode* page_node) override;

  // FrameNodeObserver:
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnFrameVisibilityChanged(const FrameNode* frame_node,
                                FrameNode::Visibility previous_value) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

 private:
  void SubmitVoteForPage(const PageNode* page_node);

  // The voting channel where votes are submitted.
  VotingChannel voting_channel_;

  // The set of page nodes that represent Side Panel contents that has not yet
  // loaded. Once they start loading, they are removed from this set as the
  // content will only get its priority increased for the initial load.
  base::flat_set<const PageNode*> side_panel_pages_;

  // The set of frame nodes that have an active vote.
  base::flat_set<const FrameNode*> frames_with_vote_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_SIDE_PANEL_LOADING_VOTER_H_
