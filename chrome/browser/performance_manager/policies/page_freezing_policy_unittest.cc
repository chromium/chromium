// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_freezing_policy.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/decorators/page_live_state_decorator_delegate_impl.h"
#include "chrome/browser/performance_manager/mechanisms/page_freezer.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/decorators/freezing_vote_decorator.h"
#include "components/performance_manager/freezing/freezing_vote_aggregator.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager {
namespace policies {

namespace {

const freezing::FreezingVote kCannotFreezeVote(
    freezing::FreezingVoteValue::kCannotFreeze,
    "cannot freeze");
const freezing::FreezingVote kCanFreezeVote(
    freezing::FreezingVoteValue::kCanFreeze,
    "can freeze");

class PageFreezingPolicyAccess : public PageFreezingPolicy {
 public:
  using PageFreezingPolicy::CannotFreezeReason;
  using PageFreezingPolicy::CannotFreezeReasonToString;
};

// Mock version of a performance_manager::mechanism::PageFreezer.
class LenientMockPageFreezer
    : public performance_manager::mechanism::PageFreezer {
 public:
  LenientMockPageFreezer() = default;
  ~LenientMockPageFreezer() override = default;
  LenientMockPageFreezer(const LenientMockPageFreezer& other) = delete;
  LenientMockPageFreezer& operator=(const LenientMockPageFreezer&) = delete;

  MOCK_METHOD1(MaybeFreezePageNodeImpl, void(const PageNode* page_node));
  MOCK_METHOD1(UnfreezePageNodeImpl, void(const PageNode* page_node));

 private:
  void MaybeFreezePageNode(const PageNode* page_node) override {
    MaybeFreezePageNodeImpl(page_node);
    PageNodeImpl::FromNode(page_node)->SetLifecycleStateForTesting(
        performance_manager::mojom::LifecycleState::kFrozen);
  }
  void UnfreezePageNode(const PageNode* page_node) override {
    UnfreezePageNodeImpl(page_node);
    PageNodeImpl::FromNode(page_node)->SetLifecycleStateForTesting(
        performance_manager::mojom::LifecycleState::kRunning);
  }
};
using MockPageFreezer = ::testing::StrictMock<LenientMockPageFreezer>;

}  // namespace

class PageFreezingPolicyTest : public GraphTestHarness {
 public:
  PageFreezingPolicyTest()
      : GraphTestHarness(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PageFreezingPolicyTest() override = default;
  PageFreezingPolicyTest(const PageFreezingPolicyTest& other) = delete;
  PageFreezingPolicyTest& operator=(const PageFreezingPolicyTest&) = delete;

  void OnGraphCreated(GraphImpl* graph) override {
    // The freezing logic relies on the existence of the page live state data.
    graph->PassToGraph(std::make_unique<PageLiveStateDecorator>(
        PageLiveStateDelegateImpl::Create()));
    graph->PassToGraph(std::make_unique<FreezingVoteDecorator>());
    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<policies::PageFreezingPolicy>();
    policy_ = policy.get();
    graph->PassToGraph(std::move(policy));

    page_node_ = CreateNode<performance_manager::PageNodeImpl>();
  }

  PageNodeImpl* page_node() { return page_node_.get(); }

  PageFreezingPolicy* policy() { return policy_; }

 private:
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node_;

  raw_ptr<PageFreezingPolicy> policy_;
};

TEST_F(PageFreezingPolicyTest, AudiblePageGetsCannotFreezeVote) {
  page_node()->SetIsAudible(true);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->freezing_vote()->reason(),
            PageFreezingPolicyAccess::CannotFreezeReasonToString(
                PageFreezingPolicyAccess::CannotFreezeReason::kAudible));
}

TEST_F(PageFreezingPolicyTest, RecentlyAudiblePageGetsCannotFreezeVote) {
  page_node()->SetIsAudible(true);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  task_env().FastForwardBy(base::Seconds(1));
  page_node()->SetIsAudible(false);
  EXPECT_EQ(
      page_node()->freezing_vote()->reason(),
      PageFreezingPolicyAccess::CannotFreezeReasonToString(
          PageFreezingPolicyAccess::CannotFreezeReason::kRecentlyAudible));
  task_env().FastForwardBy(policies::kTabAudioProtectionTime);
  EXPECT_FALSE(page_node()->freezing_vote().has_value());
}

TEST_F(PageFreezingPolicyTest, PageHoldingWeblockGetsCannotFreezeVote) {
  page_node()->SetIsHoldingWebLockForTesting(true);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->freezing_vote()->reason(),
            PageFreezingPolicyAccess::CannotFreezeReasonToString(
                PageFreezingPolicyAccess::CannotFreezeReason::kHoldingWebLock));
}

TEST_F(PageFreezingPolicyTest, PageHoldingIndexedDBLockGetsCannotFreezeVote) {
  page_node()->SetIsHoldingIndexedDBLockForTesting(true);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(
      page_node()->freezing_vote()->reason(),
      PageFreezingPolicyAccess::CannotFreezeReasonToString(
          PageFreezingPolicyAccess::CannotFreezeReason::kHoldingIndexedDBLock));
}

TEST_F(PageFreezingPolicyTest, CannotFreezeIsConnectedToUSBDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToUSBDeviceForTesting(true);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(
      page_node()->freezing_vote()->reason(),
      PageFreezingPolicyAccess::CannotFreezeReasonToString(
          PageFreezingPolicyAccess::CannotFreezeReason::kConnectedToUsbDevice));
}

TEST_F(PageFreezingPolicyTest, CannotFreezePageConnectedToBluetoothDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToBluetoothDeviceForTesting(true);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->freezing_vote()->reason(),
            PageFreezingPolicyAccess::CannotFreezeReasonToString(
                PageFreezingPolicyAccess::CannotFreezeReason::
                    kConnectedToBluetoothDevice));
}

TEST_F(PageFreezingPolicyTest, CannotFreezePageCapturingVideo) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingVideoForTesting(true);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->freezing_vote()->reason(),
            PageFreezingPolicyAccess::CannotFreezeReasonToString(
                PageFreezingPolicyAccess::CannotFreezeReason::kCapturingVideo));
}

TEST_F(PageFreezingPolicyTest, CannotFreezePageCapturingAudio) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingAudioForTesting(true);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->freezing_vote()->reason(),
            PageFreezingPolicyAccess::CannotFreezeReasonToString(
                PageFreezingPolicyAccess::CannotFreezeReason::kCapturingAudio));
}

TEST_F(PageFreezingPolicyTest, CannotFreezePageBeingMirrored) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsBeingMirroredForTesting(true);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->freezing_vote()->reason(),
            PageFreezingPolicyAccess::CannotFreezeReasonToString(
                PageFreezingPolicyAccess::CannotFreezeReason::kBeingMirrored));
}

TEST_F(PageFreezingPolicyTest, CannotFreezePageCapturingWindow) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingWindowForTesting(true);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
}

TEST_F(PageFreezingPolicyTest, CannotFreezePageCapturingDisplay) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingDisplayForTesting(true);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(
      page_node()->freezing_vote()->reason(),
      PageFreezingPolicyAccess::CannotFreezeReasonToString(
          PageFreezingPolicyAccess::CannotFreezeReason::kCapturingDisplay));
}

TEST_F(PageFreezingPolicyTest, FreezingVotes) {
  std::unique_ptr<MockPageFreezer> page_freezer =
      std::make_unique<MockPageFreezer>();
  auto* page_freezer_raw = page_freezer.get();
  policy()->SetPageFreezerForTesting(std::move(page_freezer));
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedIdle);

  EXPECT_CALL(*page_freezer_raw, MaybeFreezePageNodeImpl(page_node()));
  page_node()->set_freezing_vote(kCanFreezeVote);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);

  EXPECT_CALL(*page_freezer_raw, UnfreezePageNodeImpl(page_node()));
  page_node()->set_freezing_vote(kCannotFreezeVote);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);

  EXPECT_CALL(*page_freezer_raw, MaybeFreezePageNodeImpl(page_node()));
  page_node()->set_freezing_vote(kCanFreezeVote);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);

  EXPECT_CALL(*page_freezer_raw, UnfreezePageNodeImpl(page_node()));
  page_node()->set_freezing_vote(absl::nullopt);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);

  // Sending a kCannotFreezeVote shouldn't unfreeze the page as it's already
  // in a non-freezable state.
  page_node()->set_freezing_vote(kCannotFreezeVote);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);

  // Same for removing a kCannotFreezeVote.
  page_node()->set_freezing_vote(absl::nullopt);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
}

TEST_F(PageFreezingPolicyTest, PageNodeIsntFrozenBeforeLoadingCompletes) {
  std::unique_ptr<MockPageFreezer> page_freezer =
      std::make_unique<MockPageFreezer>();
  auto* page_freezer_raw = page_freezer.get();
  policy()->SetPageFreezerForTesting(std::move(page_freezer));
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  page_node()->set_freezing_vote(kCanFreezeVote);
  // The page freezer shouldn't be called as the page node isn't fully loaded
  // yet.
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
  EXPECT_EQ(page_node()->freezing_vote()->value(),
            freezing::FreezingVoteValue::kCanFreeze);

  EXPECT_CALL(*page_freezer_raw, MaybeFreezePageNodeImpl(page_node()));
  // A transition to the fully loaded state should cause the page node to be
  // frozen.
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
}

TEST_F(PageFreezingPolicyTest, PeriodicUnfreeze) {
  std::unique_ptr<MockPageFreezer> page_freezer =
      std::make_unique<MockPageFreezer>();
  auto* page_freezer_raw = page_freezer.get();
  policy()->SetPageFreezerForTesting(std::move(page_freezer));
  page_node()->set_freezing_vote(kCanFreezeVote);
  EXPECT_CALL(*page_freezer_raw, MaybeFreezePageNodeImpl(page_node()));
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
  EXPECT_EQ(performance_manager::mojom::LifecycleState::kFrozen,
            page_node()->lifecycle_state());

  // Ensure that the page gets unfrozen periodically.
  for (int i = 0; i < 2; ++i) {
    EXPECT_CALL(*page_freezer_raw, UnfreezePageNodeImpl(page_node()));
    task_env().FastForwardBy(
        PageFreezingPolicy::GetUnfreezeIntervalForTesting());
    ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
    EXPECT_EQ(performance_manager::mojom::LifecycleState::kRunning,
              page_node()->lifecycle_state());

    EXPECT_CALL(*page_freezer_raw, MaybeFreezePageNodeImpl(page_node()));
    task_env().FastForwardBy(
        PageFreezingPolicy::GetUnfreezeDurationForTesting());
    ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
    EXPECT_EQ(performance_manager::mojom::LifecycleState::kFrozen,
              page_node()->lifecycle_state());
  }
}

TEST_F(PageFreezingPolicyTest,
       PeriodicUnfreezeCancelledIfPageIsUnfreezeVoteChanges) {
  std::unique_ptr<MockPageFreezer> page_freezer =
      std::make_unique<MockPageFreezer>();
  auto* page_freezer_raw = page_freezer.get();
  policy()->SetPageFreezerForTesting(std::move(page_freezer));
  page_node()->set_freezing_vote(kCanFreezeVote);
  EXPECT_CALL(*page_freezer_raw, MaybeFreezePageNodeImpl(page_node()));
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
  EXPECT_EQ(performance_manager::mojom::LifecycleState::kFrozen,
            page_node()->lifecycle_state());

  task_env().FastForwardBy(PageFreezingPolicy::GetUnfreezeIntervalForTesting() /
                           2);

  EXPECT_CALL(*page_freezer_raw, UnfreezePageNodeImpl(page_node()));
  page_node()->set_freezing_vote(kCannotFreezeVote);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
  EXPECT_EQ(performance_manager::mojom::LifecycleState::kRunning,
            page_node()->lifecycle_state());

  task_env().FastForwardBy(PageFreezingPolicy::GetUnfreezeIntervalForTesting() *
                           2);
  // There should be no further call to |MaybeFreezePageNodeImpl|.
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
}

TEST_F(PageFreezingPolicyTest,
       PeriodicUnfreezeCancelledIfPageIsFreezingStateChanges) {
  std::unique_ptr<MockPageFreezer> page_freezer =
      std::make_unique<MockPageFreezer>();
  auto* page_freezer_raw = page_freezer.get();
  policy()->SetPageFreezerForTesting(std::move(page_freezer));
  page_node()->set_freezing_vote(kCanFreezeVote);
  EXPECT_CALL(*page_freezer_raw, MaybeFreezePageNodeImpl(page_node()));
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
  EXPECT_EQ(performance_manager::mojom::LifecycleState::kFrozen,
            page_node()->lifecycle_state());

  task_env().FastForwardBy(PageFreezingPolicy::GetUnfreezeIntervalForTesting() /
                           2);

  // UnfreezePageNodeImpl won't be called if the page gets unfreeze from outside
  // of PerformanceManager (e.g. if it becomes visible).
  page_node()->SetLifecycleStateForTesting(
      performance_manager::mojom::LifecycleState::kRunning);
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
  EXPECT_EQ(performance_manager::mojom::LifecycleState::kRunning,
            page_node()->lifecycle_state());

  task_env().FastForwardBy(PageFreezingPolicy::GetUnfreezeIntervalForTesting() *
                           2);
  // There should be no further call to |MaybeFreezePageNodeImpl|.
  ::testing::Mock::VerifyAndClearExpectations(page_freezer_raw);
}

}  // namespace policies
}  // namespace performance_manager
