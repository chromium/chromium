// Copyright 2022 The Chromium Authors. All rights reserved.
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

    feature_list_.InitAndEnableFeature(
        performance_manager::features::kHighEfficiencyModeAvailable);
    // This is usually called when the profile is created. Fake it here since it
    // doesn't happen in tests.
    PageDiscardingHelper::GetFromGraph(graph())->SetNoDiscardPatternsForProfile(
        static_cast<PageNode*>(page_node())->GetBrowserContextID(), {});

    auto policy = std::make_unique<HighEfficiencyModePolicy>();
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));

    page_node()->SetType(PageType::kTab);
  }

  void TearDown() override {
    graph()->TakeFromGraph(policy_);
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

  HighEfficiencyModePolicy* policy() { return policy_; }

 private:
  raw_ptr<HighEfficiencyModePolicy> policy_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(HighEfficiencyModeTest, NoDiscardIfHighEfficiencyOff) {
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  task_env().FastForwardUntilNoTasksRemain();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, DiscardAfterBackgrounded) {
  page_node()->SetIsVisible(true);
  policy()->OnHighEfficiencyModeChanged(true);

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  page_node()->SetIsVisible(false);

  task_env().FastForwardBy(
      performance_manager::features::kHighEfficiencyModeTimeBeforeDiscard
          .Get());
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
  page_node()->SetIsVisible(true);
  policy()->OnHighEfficiencyModeChanged(true);

  page_node()->SetIsAudible(true);

  page_node()->SetIsVisible(false);
  task_env().FastForwardUntilNoTasksRemain();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HighEfficiencyModeTest, DiscardIfAlreadyNotVisibleWhenModeEnabled) {
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Shouldn't be discarded yet
  task_env().FastForwardUntilNoTasksRemain();
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Advance time by the usual discard interval, minus 10 seconds. This means
  // that the page will be discarded 10 seconds after the mode is changed.
  task_env().FastForwardBy(
      performance_manager::features::kHighEfficiencyModeTimeBeforeDiscard
          .Get() -
      base::Seconds(10));
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  policy()->OnHighEfficiencyModeChanged(true);
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  task_env().FastForwardBy(base::Seconds(10));
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

}  // namespace performance_manager::policies
