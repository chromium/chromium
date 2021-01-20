// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_FREEZING_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_FREEZING_POLICY_H_

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

namespace mechanism {
class PageFreezer;
}  // namespace mechanism

namespace policies {

// A simple freezing policy that attempts to freeze pages when their associated
// freezing vote is positive.
//
// Tabs in one of the following states won't be frozen:
//   - Audible;
//   - Capturing video;
//   - Capturing audio;
//   - Mirrored;
//   - Capturing window;
//   - Capturing display;
//   - Connected to a bluetooth device;
//   - Connected to a USB device;
//   - Holding at least one IndexedDB lock;
//   - Holding at least one WebLock.
//
// Note that visible tabs can't be frozen and tabs that becomes visible are
// automatically unfrozen, there's no need to track this feature here.
class PageFreezingPolicy : public GraphOwned,
                           public PageNode::ObserverDefaultImpl,
                           public PageLiveStateObserver {
 public:
  PageFreezingPolicy();
  PageFreezingPolicy(const PageFreezingPolicy&) = delete;
  PageFreezingPolicy(PageFreezingPolicy&&) = delete;
  PageFreezingPolicy& operator=(const PageFreezingPolicy&) = delete;
  PageFreezingPolicy& operator=(PageFreezingPolicy&&) = delete;
  ~PageFreezingPolicy() override;

  void SetPageFreezerForTesting(
      std::unique_ptr<mechanism::PageFreezer> page_freezer) {
    page_freezer_ = std::move(page_freezer);
  }

 protected:
  // List of states that prevent a tab from being frozen.
  enum class CannotFreezeReason {
    kAudible,
    kHoldingIndexedDBLock,
    kHoldingWebLock,
    kConnectedToUsbDevice,
    kConnectedToBluetoothDevice,
    kCapturingVideo,
    kCapturingAudio,
    kBeingMirrored,
    kCapturingWindow,
    kCapturingDisplay,
  };

  // Helper function to convert a |CannotFreezeReason| to a string.
  static const char* CannotFreezeReasonToString(CannotFreezeReason reason);

 private:
  // A map that associates a CannotFreezeReason to a negative vote.
  using PageCannotFreezeVoteMap =
      base::flat_map<CannotFreezeReason,
                     std::unique_ptr<freezing::FreezingVotingChannelWrapper>>;
  // A map that associates a PageCannotFreezeVoteMap with a page node.
  using NegativeVotesForPagesMap =
      base::flat_map<const PageNode*, PageCannotFreezeVoteMap>;

  // Indicates if the negative freezing vote should be emitted or removed.
  enum class NegativeVoteAction {
    kEmit,
    kRemove,
  };

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsAudibleChanged(const PageNode* page_node) override;
  void OnPageIsHoldingWebLockChanged(const PageNode* page_node) override;
  void OnPageIsHoldingIndexedDBLockChanged(const PageNode* page_node) override;
  void OnFreezingVoteChanged(
      const PageNode* page_node,
      base::Optional<performance_manager::freezing::FreezingVote> previous_vote)
      override;

  // PageLiveStateObserver:
  void OnIsConnectedToUSBDeviceChanged(const PageNode* page_node) override;
  void OnIsConnectedToBluetoothDeviceChanged(
      const PageNode* page_node) override;
  void OnIsCapturingVideoChanged(const PageNode* page_node) override;
  void OnIsCapturingAudioChanged(const PageNode* page_node) override;
  void OnIsBeingMirroredChanged(const PageNode* page_node) override;
  void OnIsCapturingWindowChanged(const PageNode* page_node) override;
  void OnIsCapturingDisplayChanged(const PageNode* page_node) override;
  void OnIsAutoDiscardableChanged(const PageNode* page_node) override {}
  void OnWasDiscardedChanged(const PageNode* page_node) override {}

  // Emit or remove a negative freezing vote for |page_node| for |reason|.
  // There can only be one vote associated with this reason.
  void UpdateNegativeFreezingVote(const PageNode* page_node,
                                  CannotFreezeReason reason,
                                  NegativeVoteAction action);

  NegativeVotesForPagesMap negative_vote_for_pages_;

  Graph* graph_ = nullptr;

  // The page node being removed, used to avoid freezing/unfreezing a page node
  // while it's being removed.
  const PageNode* page_node_being_removed_ = nullptr;

  // The freezing mechanism used to do the actual freezing.
  std::unique_ptr<mechanism::PageFreezer> page_freezer_;
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_FREEZING_POLICY_H_
