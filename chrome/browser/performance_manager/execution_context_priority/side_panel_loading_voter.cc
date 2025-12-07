// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/execution_context_priority/side_panel_loading_voter.h"

#include "chrome/common/webui_url_constants.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/graph.h"
#include "ui/accessibility/accessibility_features.h"
#include "url/gurl.h"

namespace performance_manager::execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  auto* registry = execution_context::ExecutionContextRegistry::GetFromGraph(
      frame_node->GetGraph());
  CHECK(registry);
  return registry->GetExecutionContextForFrameNode(frame_node);
}

}  // namespace

// static
const char SidePanelLoadingVoter::kSidePanelLoadingReason[] =
    "Side Panel loading";

SidePanelLoadingVoter::SidePanelLoadingVoter() = default;

SidePanelLoadingVoter::~SidePanelLoadingVoter() = default;

void SidePanelLoadingVoter::MarkAsSidePanel(const PageNode* page_node) {
  CHECK(page_node->GetMainFrameNode());

  // This is possible for a preloaded Side Panel. The navigation has already
  // committed and the page is visible.
  if (!page_node->GetMainFrameUrl().is_empty()) {
    if (!features::IsImmersiveReadAnythingEnabled()) {
      CHECK(page_node->IsVisible());
      return;
    }
    // If the Side Panel is a Reading Mode and Immersive Reading Mode is
    // enabled, don't CHECK if page_node->IsVisible(), because a preloaded WebUI
    // is expected when a user is switching between Immersive Reading Mode and
    // the Side Panel.
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
  // Clean up outstanding votes, which is possible if the graph is tore down
  // while a Side Panel is loading.
  for (const FrameNode* frame_node : frames_with_vote_) {
    voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
  }
  frames_with_vote_.clear();

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
      SubmitVoteForPage(page_node);
    }
    return;
  }
}

void SidePanelLoadingVoter::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  // Check if a frame with an outstanding vote is being removed.
  size_t removed = frames_with_vote_.erase(frame_node);
  if (removed) {
    voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
  }
}

void SidePanelLoadingVoter::OnFrameVisibilityChanged(
    const FrameNode* frame_node,
    FrameNode::Visibility previous_value) {
  // Ignore visibility changed events where the frame is not visible.
  if (frame_node->GetVisibility() == FrameNode::Visibility::kNotVisible) {
    return;
  }

  // Check if a frame with an outstanding vote just became visible.
  size_t removed = frames_with_vote_.erase(frame_node);
  if (removed) {
    // The side panel is visible, no longer need to increase priority.
    voting_channel_.InvalidateVote(GetExecutionContext(frame_node));
  }
}

void SidePanelLoadingVoter::SubmitVoteForPage(const PageNode* page_node) {
  CHECK(!page_node->IsVisible());

  // We only need to increase the priority of the main frame.
  const FrameNode* frame_node = page_node->GetMainFrameNode();

  auto [_, inserted] = frames_with_vote_.insert(frame_node);
  CHECK(inserted);

  voting_channel_.SubmitVote(
      GetExecutionContext(page_node->GetMainFrameNode()),
      Vote(base::TaskPriority::USER_BLOCKING, kSidePanelLoadingReason));
}

}  // namespace performance_manager::execution_context_priority
