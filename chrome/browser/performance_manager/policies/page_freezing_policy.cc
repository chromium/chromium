// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_freezing_policy.h"

#include <memory>

#include "base/stl_util.h"
#include "chrome/browser/performance_manager/mechanisms/page_freezer.h"
#include "components/performance_manager/freezing/freezing_vote_aggregator.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom-shared.h"

namespace performance_manager {
namespace policies {

namespace {

bool IsPageNodeFrozen(const PageNode* page_node) {
  return page_node->GetLifecycleState() ==
         performance_manager::mojom::LifecycleState::kFrozen;
}

}  // namespace

PageFreezingPolicy::PageFreezingPolicy()
    : page_freezer_(std::make_unique<mechanism::PageFreezer>()) {}
PageFreezingPolicy::~PageFreezingPolicy() = default;

void PageFreezingPolicy::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
  graph->AddPageNodeObserver(this);
}

void PageFreezingPolicy::OnTakenFromGraph(Graph* graph) {
  graph->RemovePageNodeObserver(this);
  graph_ = nullptr;
}

void PageFreezingPolicy::OnPageNodeAdded(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)->AddObserver(
      this);
  negative_vote_for_pages_.emplace(page_node, PageCannotFreezeVoteMap());

  if (page_node->IsAudible())
    OnIsAudibleChanged(page_node);
}

void PageFreezingPolicy::OnBeforePageNodeRemoved(const PageNode* page_node) {
  page_node_being_removed_ = page_node;
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->RemoveObserver(this);
  negative_vote_for_pages_.erase(page_node);
  page_node_being_removed_ = nullptr;
}

void PageFreezingPolicy::OnIsAudibleChanged(const PageNode* page_node) {
  UpdateNegativeFreezingVote(page_node, CannotFreezeReason::kAudible,
                             page_node->IsAudible()
                                 ? NegativeVoteAction::kEmit
                                 : NegativeVoteAction::kRemove);
}

void PageFreezingPolicy::OnPageIsHoldingWebLockChanged(
    const PageNode* page_node) {
  UpdateNegativeFreezingVote(page_node, CannotFreezeReason::kHoldingWebLock,
                             page_node->IsHoldingWebLock()
                                 ? NegativeVoteAction::kEmit
                                 : NegativeVoteAction::kRemove);
}

void PageFreezingPolicy::OnPageIsHoldingIndexedDBLockChanged(
    const PageNode* page_node) {
  UpdateNegativeFreezingVote(
      page_node, CannotFreezeReason::kHoldingIndexedDBLock,
      page_node->IsHoldingIndexedDBLock() ? NegativeVoteAction::kEmit
                                          : NegativeVoteAction::kRemove);
}

void PageFreezingPolicy::OnFreezingVoteChanged(
    const PageNode* page_node,
    base::Optional<performance_manager::freezing::FreezingVote> previous_vote) {
  if (page_node == page_node_being_removed_)
    return;

  auto freezing_vote = page_node->GetFreezingVote();

  // Unfreeze the page if the freezing vote becomes negative or invalid, and was
  // previously positive.
  if (!freezing_vote.has_value() ||
      freezing_vote->value() == freezing::FreezingVoteValue::kCannotFreeze) {
    if (previous_vote.has_value() &&
        previous_vote->value() == freezing::FreezingVoteValue::kCanFreeze) {
      // Don't check if the page is actually frozen before sending the unfreeze
      // event as it's not guaranteed that the freezing state will be properly
      // reflected in PerformanceManager before the vote gets invalidated (e.g.
      // if the vote has a really short lifetime).
      page_freezer_->UnfreezePageNode(page_node);
    }
  } else {
    DCHECK_EQ(freezing::FreezingVoteValue::kCanFreeze, freezing_vote->value());
    if (!IsPageNodeFrozen(page_node)) {
      page_freezer_->MaybeFreezePageNode(page_node);
    }
  }
}

void PageFreezingPolicy::OnIsConnectedToUSBDeviceChanged(
    const PageNode* page_node) {
  UpdateNegativeFreezingVote(
      page_node, CannotFreezeReason::kConnectedToUsbDevice,
      PageLiveStateDecorator::Data::FromPageNode(page_node)
              ->IsConnectedToUSBDevice()
          ? NegativeVoteAction::kEmit
          : NegativeVoteAction::kRemove);
}

void PageFreezingPolicy::OnIsConnectedToBluetoothDeviceChanged(
    const PageNode* page_node) {
  UpdateNegativeFreezingVote(
      page_node, CannotFreezeReason::kConnectedToBluetoothDevice,
      PageLiveStateDecorator::Data::FromPageNode(page_node)
              ->IsConnectedToBluetoothDevice()
          ? NegativeVoteAction::kEmit
          : NegativeVoteAction::kRemove);
}

void PageFreezingPolicy::OnIsCapturingVideoChanged(const PageNode* page_node) {
  UpdateNegativeFreezingVote(
      page_node, CannotFreezeReason::kCapturingVideo,
      PageLiveStateDecorator::Data::FromPageNode(page_node)->IsCapturingVideo()
          ? NegativeVoteAction::kEmit
          : NegativeVoteAction::kRemove);
}

void PageFreezingPolicy::OnIsCapturingAudioChanged(const PageNode* page_node) {
  UpdateNegativeFreezingVote(
      page_node, CannotFreezeReason::kCapturingAudio,
      PageLiveStateDecorator::Data::FromPageNode(page_node)->IsCapturingAudio()
          ? NegativeVoteAction::kEmit
          : NegativeVoteAction::kRemove);
}

void PageFreezingPolicy::OnIsBeingMirroredChanged(const PageNode* page_node) {
  UpdateNegativeFreezingVote(
      page_node, CannotFreezeReason::kBeingMirrored,
      PageLiveStateDecorator::Data::FromPageNode(page_node)->IsBeingMirrored()
          ? NegativeVoteAction::kEmit
          : NegativeVoteAction::kRemove);
}

void PageFreezingPolicy::OnIsCapturingWindowChanged(const PageNode* page_node) {
  UpdateNegativeFreezingVote(
      page_node, CannotFreezeReason::kCapturingWindow,
      PageLiveStateDecorator::Data::FromPageNode(page_node)->IsCapturingWindow()
          ? NegativeVoteAction::kEmit
          : NegativeVoteAction::kRemove);
}

void PageFreezingPolicy::OnIsCapturingDisplayChanged(
    const PageNode* page_node) {
  UpdateNegativeFreezingVote(
      page_node, CannotFreezeReason::kCapturingDisplay,
      PageLiveStateDecorator::Data::FromPageNode(page_node)
              ->IsCapturingDisplay()
          ? NegativeVoteAction::kEmit
          : NegativeVoteAction::kRemove);
}

// static
const char* PageFreezingPolicy::CannotFreezeReasonToString(
    CannotFreezeReason reason) {
  switch (reason) {
    case CannotFreezeReason::kAudible:
      return "Page is audible";
    case CannotFreezeReason::kHoldingIndexedDBLock:
      return "Page is holding an IndexedDB lock";
    case CannotFreezeReason::kHoldingWebLock:
      return "Page is holding a Web Lock";
    case CannotFreezeReason::kConnectedToUsbDevice:
      return "Page is connected to a USB device";
    case CannotFreezeReason::kConnectedToBluetoothDevice:
      return "Page is connected to a Bluetooth device";
    case CannotFreezeReason::kCapturingVideo:
      return "Page is capturing video";
    case CannotFreezeReason::kCapturingAudio:
      return "Page is capturing audio";
    case CannotFreezeReason::kBeingMirrored:
      return "Page is being mirrored";
    case CannotFreezeReason::kCapturingWindow:
      return "Page is capturing window";
    case CannotFreezeReason::kCapturingDisplay:
      return "Page is capturing display";
  }
}

void PageFreezingPolicy::UpdateNegativeFreezingVote(const PageNode* page_node,
                                                    CannotFreezeReason reason,
                                                    NegativeVoteAction action) {
  auto negative_vote_for_pages_iter = negative_vote_for_pages_.find(page_node);
  CHECK(negative_vote_for_pages_iter != negative_vote_for_pages_.end());
  auto& negative_votes = negative_vote_for_pages_iter->second;

  if (action == NegativeVoteAction::kEmit) {
    // Check if we already have a vote for this reason, if so this vote should
    // be in an invalid state.
    auto iter = negative_votes.find(reason);
    if (iter == negative_votes.end()) {
      // Add the vote for |reason| if not already present.
      iter =
          negative_votes
              .insert(std::make_pair(
                  reason,
                  std::make_unique<freezing::FreezingVotingChannelWrapper>()))
              .first;
      iter->second->SetVotingChannel(
          page_node->GetGraph()
              ->GetRegisteredObjectAs<freezing::FreezingVoteAggregator>()
              ->GetVotingChannel());
    } else {
      DCHECK(!iter->second->HasVoteForContext(page_node));
    }
    // Submit the negative freezing vote.
    iter->second->SubmitVote(page_node,
                             {freezing::FreezingVoteValue::kCannotFreeze,
                              CannotFreezeReasonToString(reason)});
  } else {
    // Invalidate the vote rather than removing it to avoid having to recreate
    // it multiple times.
    DCHECK(base::Contains(negative_votes, reason));
    negative_votes[reason]->InvalidateVote(page_node);
  }
}

}  // namespace policies
}  // namespace performance_manager
