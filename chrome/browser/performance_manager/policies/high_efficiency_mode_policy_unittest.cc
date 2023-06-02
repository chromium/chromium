// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"

namespace performance_manager::policies {

class HighEfficiencyModeTest
    : public testing::GraphTestHarnessWithMockDiscarder {
 public:
  void SetUp() override {
    testing::GraphTestHarnessWithMockDiscarder::SetUp();

    // This is usually called when the profile is created. Fake it here since it
    // doesn't happen in tests.
    PageDiscardingHelper::GetFromGraph(graph())->SetNoDiscardPatternsForProfile(
        static_cast<PageNode*>(page_node())->GetBrowserContextID(), {});

    auto policy = std::make_unique<HighEfficiencyModePolicy>();
    policy->SetTimeBeforeDiscard(base::Hours(2));
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));
  }

  void TearDown() override {
    graph()->TakeFromGraph(policy_);
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

  HighEfficiencyModePolicy* policy() { return policy_; }

 protected:
  PageNodeImpl* CreateOtherPageNode() {
    other_process_node_ = CreateNode<performance_manager::ProcessNodeImpl>();
    other_page_node_ = CreateNode<performance_manager::PageNodeImpl>();
    other_main_frame_node_ = CreateFrameNodeAutoId(other_process_node_.get(),
                                                   other_page_node_.get());
    other_main_frame_node_->SetIsCurrent(true);
    testing::MakePageNodeDiscardable(other_page_node_.get(), task_env());

    return other_page_node_.get();
  }

 private:
  raw_ptr<HighEfficiencyModePolicy, DanglingUntriaged> policy_;

  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      other_page_node_;
  performance_manager::TestNodeWrapper<performance_manager::ProcessNodeImpl>
      other_process_node_;
  performance_manager::TestNodeWrapper<performance_manager::FrameNodeImpl>
      other_main_frame_node_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(HighEfficiencyModeTest, NoDiscardIfHighEfficiencyOff) {
  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  task_env().FastForwardUntilNoTasksRemain();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, DiscardAfterBackgrounded) {
  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  policy()->OnHighEfficiencyModeChanged(true);

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  page_node()->SetIsVisible(false);

  task_env().FastForwardBy(policy()->GetTimeBeforeDiscardForTesting());
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, DontDiscardAfterBackgroundedIfSuspended) {
  policy()->SetTimeBeforeDiscard(base::Hours(2));
  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  policy()->OnHighEfficiencyModeChanged(true);
  page_node()->SetIsVisible(false);

  EXPECT_EQ(policy()->GetTimeBeforeDiscardForTesting(), base::Hours(2));

  // The tab isn't discarded if the elapsed time was spent with the device
  // suspended.
  task_env().SuspendedFastForwardBy(base::Hours(10));
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Advance only one hour, there should still not be a discard.
  task_env().FastForwardBy(base::Hours(1));
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Suspend again for more than the expected time, no discard should happen.
  task_env().SuspendedFastForwardBy(base::Hours(10));
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));

  // Finally advance un-suspended until the time is elapsed, the tab should be
  // discarded.
  task_env().FastForwardBy(base::Hours(1));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, DontDiscardIfPageIsNotATab) {
  page_node()->SetType(PageType::kUnknown);
  policy()->OnHighEfficiencyModeChanged(true);
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  task_env().FastForwardUntilNoTasksRemain();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

// The tab shouldn't be discarded if it's playing audio. There are many other
// conditions that prevent discarding, but they're implemented in
// `PageDiscardingHelper` and therefore tested there.
TEST_F(HighEfficiencyModeTest, DontDiscardIfPlayingAudio) {
  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  policy()->OnHighEfficiencyModeChanged(true);

  page_node()->SetIsAudible(true);

  page_node()->SetIsVisible(false);
  task_env().FastForwardUntilNoTasksRemain();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, TimeBeforeDiscardChangedBeforeTimerStarted) {
  base::TimeDelta original_time_before_discard =
      policy()->GetTimeBeforeDiscardForTesting();
  base::TimeDelta increased_time_before_discard = base::Seconds(10);
  policy()->SetTimeBeforeDiscard(original_time_before_discard +
                                 increased_time_before_discard);

  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  policy()->OnHighEfficiencyModeChanged(true);

  task_env().FastForwardBy(original_time_before_discard);
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));

  task_env().FastForwardBy(increased_time_before_discard);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, TimeBeforeDiscardReduced) {
  base::TimeDelta original_time_before_discard =
      policy()->GetTimeBeforeDiscardForTesting();
  constexpr base::TimeDelta kNewTimeBeforeDiscard = base::Minutes(20);
  constexpr base::TimeDelta kInitialBackgroundTime = base::Minutes(10);
  EXPECT_GE(original_time_before_discard, kNewTimeBeforeDiscard);
  EXPECT_GE(kNewTimeBeforeDiscard, kInitialBackgroundTime);

  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  policy()->OnHighEfficiencyModeChanged(true);

  task_env().FastForwardBy(kInitialBackgroundTime);
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  policy()->SetTimeBeforeDiscard(kNewTimeBeforeDiscard);

  // Expect tab to not take into account time spent in the background prior
  // to the time before discard changing.
  task_env().FastForwardBy(kNewTimeBeforeDiscard - kInitialBackgroundTime);
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Expect tab to be discarded after the new time before discard has elapsed
  // since the last change to it.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));

  task_env().FastForwardBy(kInitialBackgroundTime);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, TimeBeforeDiscardReducedBelowBackgroundedTime) {
  base::TimeDelta original_time_before_discard =
      policy()->GetTimeBeforeDiscardForTesting();
  constexpr base::TimeDelta kNewTimeBeforeDiscard = base::Minutes(5);
  constexpr base::TimeDelta kInitialBackgroundTime = base::Minutes(10);
  EXPECT_GE(original_time_before_discard, kInitialBackgroundTime);
  EXPECT_GE(kInitialBackgroundTime, kNewTimeBeforeDiscard);

  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  policy()->OnHighEfficiencyModeChanged(true);

  task_env().FastForwardBy(kInitialBackgroundTime);
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Expect tab to not be immediately discarded if time to discard is changed
  // to something smaller than the already elapsed time in the background.
  policy()->SetTimeBeforeDiscard(kNewTimeBeforeDiscard);
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Expect tab to be discarded after the new time before discard has elapsed
  // since the last change to it.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));

  task_env().FastForwardBy(kNewTimeBeforeDiscard);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, TimeBeforeDiscardIncreased) {
  base::TimeDelta original_time_before_discard =
      policy()->GetTimeBeforeDiscardForTesting();
  constexpr base::TimeDelta kNewTimeBeforeDiscard = base::Hours(3);
  constexpr base::TimeDelta kInitialBackgroundTime = base::Minutes(10);
  EXPECT_GE(kNewTimeBeforeDiscard, original_time_before_discard);
  EXPECT_GE(original_time_before_discard, kInitialBackgroundTime);

  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  policy()->OnHighEfficiencyModeChanged(true);

  task_env().FastForwardBy(kInitialBackgroundTime);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  // Time elapsed since beginning of test = kInitialBackgroundTime

  policy()->SetTimeBeforeDiscard(kNewTimeBeforeDiscard);

  // Expect original timer to not be in effect
  task_env().FastForwardBy(original_time_before_discard -
                           kInitialBackgroundTime);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  // Time elapsed since beginning of test = original_time_before_discard

  // Expect tab to not take into account time spent in the background prior to
  // the time before discard changing.
  task_env().FastForwardBy(kNewTimeBeforeDiscard -
                           original_time_before_discard);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  // Time elapsed since beginning of test = kNewTimeBeforeDiscard

  // Expect tab to be discarded after the new time before discard has elapsed
  // since the last change to it.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  task_env().FastForwardBy(kInitialBackgroundTime);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  // Time elapsed since beginning of test = kInitialBackgroundTime +
  //                                        kNewTimeBeforeDiscard
}

TEST_F(HighEfficiencyModeTest, DontDiscardIfAlreadyNotVisibleWhenModeEnabled) {
  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Shouldn't be discarded yet
  task_env().FastForwardUntilNoTasksRemain();
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Advance time by the usual discard interval, minus 10 seconds.
  task_env().FastForwardBy(policy()->GetTimeBeforeDiscardForTesting() -
                           base::Seconds(10));
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  policy()->OnHighEfficiencyModeChanged(true);

  // The page should not be discarded 10 seconds after the mode is changed.
  task_env().FastForwardBy(base::Seconds(10));
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Instead, it should be discarded after the usual discard interval.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  task_env().FastForwardBy(policy()->GetTimeBeforeDiscardForTesting() -
                           base::Seconds(10));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, NoDiscardIfPageNodeRemoved) {
  page_node()->SetType(PageType::kTab);
  page_node()->SetIsVisible(true);
  policy()->OnHighEfficiencyModeChanged(true);

  page_node()->SetIsVisible(false);
  policy()->OnBeforePageNodeRemoved(page_node());

  task_env().FastForwardUntilNoTasksRemain();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, UnknownPageNodeNeverAddedToMap) {
  // This case will be using a different page node, so make the default one
  // visible so it's not discarded.
  page_node()->SetIsVisible(true);
  policy()->OnHighEfficiencyModeChanged(true);

  PageNodeImpl* page_node = CreateOtherPageNode();
  EXPECT_EQ(PageType::kUnknown, page_node->type());

  page_node->SetIsVisible(false);
  policy()->OnBeforePageNodeRemoved(page_node);

  task_env().FastForwardUntilNoTasksRemain();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, PageNodeDiscardedIfTypeChanges) {
  // This case will be using a different page node, so make the default one
  // visible so it's not discarded.
  page_node()->SetIsVisible(true);
  policy()->OnHighEfficiencyModeChanged(true);

  PageNodeImpl* page_node = CreateOtherPageNode();
  EXPECT_EQ(PageType::kUnknown, page_node->type());

  page_node->SetType(PageType::kTab);

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node))
      .WillOnce(::testing::Return(true));
  page_node->SetIsVisible(false);

  task_env().FastForwardBy(policy()->GetTimeBeforeDiscardForTesting());
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, PageNodeNotDiscardedIfBecomesNotTab) {
  // This case will be using a different page node, so make the default one
  // visible so it's not discarded.
  page_node()->SetIsVisible(true);
  policy()->OnHighEfficiencyModeChanged(true);

  PageNodeImpl* page_node = CreateOtherPageNode();
  page_node->SetType(PageType::kTab);

  page_node->SetIsVisible(false);
  page_node->SetType(PageType::kUnknown);

  task_env().FastForwardUntilNoTasksRemain();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

}  // namespace performance_manager::policies
