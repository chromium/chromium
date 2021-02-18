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

bool IsPageConnectedToUSBDevice(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsConnectedToUSBDevice();
}

bool IsPageConnectedToBluetoothDevice(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsConnectedToBluetoothDevice();
}

bool IsPageCapturingVideo(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsCapturingVideo();
}

bool IsPageCapturingAudio(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsCapturingAudio();
}

bool IsPageBeingMirrored(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsBeingMirrored();
}

bool IsPageCapturingWindow(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsCapturingWindow();
}

bool IsPageCapturingDisplay(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsCapturingDisplay();
}

}  // namespace

PageFreezingPolicy::PageFreezingPolicy()
    : page_freezer_(std::make_unique<mechanism::PageFreezer>()) {}
PageFreezingPolicy::~PageFreezingPolicy() = default;

void PageFreezingPolicy::OnBeforeGraphDestroyed(Graph* graph) {
  graph->RemovePageNodeObserver(this);
  graph->RemoveGraphObserver(this);

  // Clean up voting channels here as it must be done before the aggregator is
  // torn down, which may happen before our OnTakenFromGraph() would be called.
  for (int i = 0; i < CannotFreezeReason::kCount; ++i)
    voting_channels_[i].Reset();
}

void PageFreezingPolicy::OnPassedToGraph(Graph* graph) {
  for (int i = 0; i < CannotFreezeReason::kCount; ++i) {
    voting_channels_[i] =
        graph->GetRegisteredObjectAs<freezing::FreezingVoteAggregator>()
            ->GetVotingChannel();
  }

  graph->AddGraphObserver(this);
  graph->AddPageNodeObserver(this);
}

void PageFreezingPolicy::OnPageNodeAdded(const PageNode* page_node) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)->AddObserver(
      this);

  if (page_node->IsAudible())
    SubmitNegativeFreezingVote(page_node, kAudible);

  DCHECK(!page_node->IsHoldingWebLock());
  DCHECK(!page_node->IsHoldingIndexedDBLock());
  DCHECK(!IsPageConnectedToUSBDevice(page_node));
  DCHECK(!IsPageConnectedToBluetoothDevice(page_node));
  DCHECK(!IsPageCapturingVideo(page_node));
  DCHECK(!IsPageCapturingAudio(page_node));
  DCHECK(!IsPageBeingMirrored(page_node));
  DCHECK(!IsPageCapturingWindow(page_node));
  DCHECK(!IsPageCapturingDisplay(page_node));
}

void PageFreezingPolicy::OnBeforePageNodeRemoved(const PageNode* page_node) {
  page_node_being_removed_ = page_node;
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->RemoveObserver(this);

  if (page_node->IsAudible())
    InvalidateNegativeFreezingVote(page_node, kAudible);

  if (page_node->IsHoldingWebLock())
    InvalidateNegativeFreezingVote(page_node, kHoldingWebLock);

  if (page_node->IsHoldingIndexedDBLock())
    InvalidateNegativeFreezingVote(page_node, kHoldingIndexedDBLock);

  if (IsPageConnectedToUSBDevice(page_node))
    InvalidateNegativeFreezingVote(page_node, kConnectedToUsbDevice);

  if (IsPageConnectedToBluetoothDevice(page_node))
    InvalidateNegativeFreezingVote(page_node, kConnectedToBluetoothDevice);

  if (IsPageCapturingVideo(page_node))
    InvalidateNegativeFreezingVote(page_node, kCapturingVideo);

  if (IsPageCapturingAudio(page_node))
    InvalidateNegativeFreezingVote(page_node, kCapturingAudio);

  if (IsPageBeingMirrored(page_node))
    InvalidateNegativeFreezingVote(page_node, kBeingMirrored);

  if (IsPageCapturingWindow(page_node))
    InvalidateNegativeFreezingVote(page_node, kCapturingWindow);

  if (IsPageCapturingDisplay(page_node))
    InvalidateNegativeFreezingVote(page_node, kCapturingDisplay);

  page_node_being_removed_ = nullptr;
}

void PageFreezingPolicy::OnIsAudibleChanged(const PageNode* page_node) {
  OnPropertyChanged(page_node, page_node->IsAudible(),
                    CannotFreezeReason::kAudible);
}

void PageFreezingPolicy::OnPageIsHoldingWebLockChanged(
    const PageNode* page_node) {
  OnPropertyChanged(page_node, page_node->IsHoldingWebLock(),
                    CannotFreezeReason::kHoldingWebLock);
}

void PageFreezingPolicy::OnPageIsHoldingIndexedDBLockChanged(
    const PageNode* page_node) {
  OnPropertyChanged(page_node, page_node->IsHoldingIndexedDBLock(),
                    CannotFreezeReason::kHoldingIndexedDBLock);
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

    // Don't attempt to freeze a page if it's not fully loaded yet.
    if (page_node->GetLoadingState() != PageNode::LoadingState::kLoadedIdle)
      return;

    if (!IsPageNodeFrozen(page_node)) {
      page_freezer_->MaybeFreezePageNode(page_node);
    }
  }
}

void PageFreezingPolicy::OnLoadingStateChanged(const PageNode* page_node) {
  if (page_node->GetLoadingState() != PageNode::LoadingState::kLoadedIdle)
    return;
  auto freezing_vote = page_node->GetFreezingVote();
  if (freezing_vote.has_value() &&
      freezing_vote->value() == freezing::FreezingVoteValue::kCanFreeze) {
    page_freezer_->MaybeFreezePageNode(page_node);
  }
}

void PageFreezingPolicy::OnIsConnectedToUSBDeviceChanged(
    const PageNode* page_node) {
  OnPropertyChanged(page_node, IsPageConnectedToUSBDevice(page_node),
                    CannotFreezeReason::kConnectedToUsbDevice);
}

void PageFreezingPolicy::OnIsConnectedToBluetoothDeviceChanged(
    const PageNode* page_node) {
  OnPropertyChanged(page_node, IsPageConnectedToBluetoothDevice(page_node),
                    CannotFreezeReason::kConnectedToBluetoothDevice);
}

void PageFreezingPolicy::OnIsCapturingVideoChanged(const PageNode* page_node) {
  OnPropertyChanged(page_node, IsPageCapturingVideo(page_node),
                    CannotFreezeReason::kCapturingVideo);
}

void PageFreezingPolicy::OnIsCapturingAudioChanged(const PageNode* page_node) {
  OnPropertyChanged(page_node, IsPageCapturingAudio(page_node),
                    CannotFreezeReason::kCapturingAudio);
}

void PageFreezingPolicy::OnIsBeingMirroredChanged(const PageNode* page_node) {
  OnPropertyChanged(page_node, IsPageBeingMirrored(page_node),
                    CannotFreezeReason::kBeingMirrored);
}

void PageFreezingPolicy::OnIsCapturingWindowChanged(const PageNode* page_node) {
  OnPropertyChanged(page_node, IsPageCapturingWindow(page_node),
                    CannotFreezeReason::kCapturingWindow);
}

void PageFreezingPolicy::OnIsCapturingDisplayChanged(
    const PageNode* page_node) {
  OnPropertyChanged(page_node, IsPageCapturingDisplay(page_node),
                    CannotFreezeReason::kCapturingDisplay);
}

void PageFreezingPolicy::OnPropertyChanged(const PageNode* page_node,
                                           bool submit_vote,
                                           CannotFreezeReason reason) {
  if (submit_vote) {
    SubmitNegativeFreezingVote(page_node, reason);
  } else {
    InvalidateNegativeFreezingVote(page_node, reason);
  }
}

// static
const char* PageFreezingPolicy::CannotFreezeReasonToString(
    CannotFreezeReason reason) {
  switch (reason) {
    case CannotFreezeReason::kAudible:
      return "Page is audible";
    case CannotFreezeReason::kHoldingWebLock:
      return "Page is holding a Web Lock";
    case CannotFreezeReason::kHoldingIndexedDBLock:
      return "Page is holding an IndexedDB lock";
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
    case CannotFreezeReason::kCount:
      NOTREACHED();
      return "";
  }
}

void PageFreezingPolicy::SubmitNegativeFreezingVote(const PageNode* page_node,
                                                    CannotFreezeReason reason) {
  freezing::FreezingVote vote(freezing::FreezingVoteValue::kCannotFreeze,
                              CannotFreezeReasonToString(reason));
  voting_channels_[reason].SubmitVote(page_node, vote);
}

void PageFreezingPolicy::InvalidateNegativeFreezingVote(
    const PageNode* page_node,
    CannotFreezeReason reason) {
  voting_channels_[reason].InvalidateVote(page_node);
}

}  // namespace policies
}  // namespace performance_manager
