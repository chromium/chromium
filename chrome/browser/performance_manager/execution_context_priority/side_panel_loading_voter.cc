// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/execution_context_priority/side_panel_loading_voter.h"

#include "chrome/common/webui_url_constants.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "url/gurl.h"

namespace performance_manager::execution_context_priority {

// static
const char SidePanelLoadingVoter::kSidePanelLoadingReason[] =
    "Side Panel loading";

SidePanelLoadingVoter::SidePanelLoadingVoter() = default;

SidePanelLoadingVoter::~SidePanelLoadingVoter() = default;

void SidePanelLoadingVoter::MarkAsSidePanel(const PageNode* page_node) {
  CHECK(page_node->GetPrimaryMainFrameNode());

  // This is possible for a preloaded Side Panel. The navigation has already
  // committed and the page is visible.
  if (!page_node->GetMainFrameUrl().is_empty()) {
    if (page_node->GetMainFrameUrl() !=
        GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL)) {
      CHECK(page_node->IsVisible());
    }
    return;
  }

  bool inserted = side_panel_pages_.insert(page_node).second;
  CHECK(inserted);
}

void SidePanelLoadingVoter::InitializeOnGraph(Graph* graph,
                                              VotingChannel voting_channel) {
  voting_channel_ = std::move(voting_channel);

  graph->RegisterObject(this);
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
}

void SidePanelLoadingVoter::TearDownOnGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->UnregisterObject(this);

  voting_channel_.Reset();
}

void SidePanelLoadingVoter::OnBeforePageNodeRemoved(const PageNode* page_node) {
  side_panel_pages_.erase(page_node);
}

void SidePanelLoadingVoter::OnMainFrameDocumentChanged(
    const PageNode* page_node) {
  // Check if a navigation committed for a Side Panel. The `page_node` is
  // removed from the set as we only increase the priority for the initial load.
  size_t removed = side_panel_pages_.erase(page_node);
  if (removed) {
    // A Side Panel just started loading. Increase its priority until it is made
    // visible.
    if (!page_node->IsVisible()) {
      SetVoteForPage(page_node);
    }
    return;
  }
}

void SidePanelLoadingVoter::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  voting_channel_.SetVote(frame_node, std::nullopt);
}

void SidePanelLoadingVoter::OnFrameVisibilityChanged(
    const FrameNode* frame_node,
    FrameNode::Visibility previous_value) {
  // When the frame becomes visible, no longer need to increase priority.
  if (frame_node->GetVisibility() != FrameNode::Visibility::kNotVisible) {
    voting_channel_.SetVote(frame_node, std::nullopt);
  }
}

void SidePanelLoadingVoter::SetVoteForPage(const PageNode* page_node) {
  CHECK(!page_node->IsVisible());

  // We only need to increase the priority of the main frame.
  // TODO(crbug.com/540480785): Subframes should also get increased priority,
  // otherwise parts of the page wouldn't be boosted.
  const FrameNode* frame_node = page_node->GetPrimaryMainFrameNode();
  CHECK(frame_node);

  voting_channel_.SetVote(
      frame_node,
      Vote(base::Process::Priority::kUserBlocking, kSidePanelLoadingReason));
}

}  // namespace performance_manager::execution_context_priority
