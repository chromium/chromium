// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/heuristic_memory_saver_policy.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "components/performance_manager/public/features.h"

namespace performance_manager::policies {

const uint64_t kDefaultAvailableMemoryValue = 60;
const uint64_t kDefaultTotalMemoryValue = 100;

const base::TimeDelta kDefaultHeartbeatInterval = base::Seconds(10);
const base::TimeDelta kDefaultMinimumTimeInBackground = base::Seconds(11);

class HeuristicMemorySaverPolicyTest
    : public testing::GraphTestHarnessWithMockDiscarder {
 protected:
  void SetUp() override {
    testing::GraphTestHarnessWithMockDiscarder::SetUp();

    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {performance_manager::features::kHighEfficiencyModeAvailable,
         performance_manager::features::kHeuristicMemorySaver},
        /*disabled_features=*/{});
    // This is usually called when the profile is created. Fake it here since it
    // doesn't happen in tests.
    PageDiscardingHelper::GetFromGraph(graph())->SetNoDiscardPatternsForProfile(
        static_cast<PageNode*>(page_node())->GetBrowserContextID(), {});
  }

  void TearDown() override {
    graph()->TakeFromGraph(policy_);
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

  // Creates the policy by forwarding it the arguments and passing it to the
  // graph, with a default set of functions for memory measurements that
  // always return 60 and 100.
  void CreatePolicy(
      uint64_t pmf_threshold_percent,
      base::TimeDelta heartbeat_interval,
      base::TimeDelta minimum_time_in_background,
      HeuristicMemorySaverPolicy::AvailableMemoryCallback
          available_memory_callback = base::BindRepeating([]() {
            return kDefaultAvailableMemoryValue;
          }),
      HeuristicMemorySaverPolicy::TotalMemoryCallback total_memory_callback =
          base::BindRepeating([]() { return kDefaultTotalMemoryValue; })) {
    auto policy = std::make_unique<HeuristicMemorySaverPolicy>(
        pmf_threshold_percent, heartbeat_interval, minimum_time_in_background,
        available_memory_callback, total_memory_callback);
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));
  }

  PageNodeImpl* CreateOtherPageNode() {
    other_process_node_ = CreateNode<performance_manager::ProcessNodeImpl>();
    other_page_node_ = CreateNode<performance_manager::PageNodeImpl>();
    other_main_frame_node_ = CreateFrameNodeAutoId(other_process_node_.get(),
                                                   other_page_node_.get());
    other_main_frame_node_->SetIsCurrent(true);
    testing::MakePageNodeDiscardable(other_page_node_.get(), task_env());

    return other_page_node_.get();
  }

  HeuristicMemorySaverPolicy* policy() { return policy_; }

 private:
  // Owned by the graph.
  raw_ptr<HeuristicMemorySaverPolicy> policy_;

  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      other_page_node_;
  performance_manager::TestNodeWrapper<performance_manager::ProcessNodeImpl>
      other_process_node_;
  performance_manager::TestNodeWrapper<performance_manager::FrameNodeImpl>
      other_main_frame_node_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(HeuristicMemorySaverPolicyTest, NoDiscardIfPolicyInactive) {
  CreatePolicy(/*pmf_threshold_percent=*/100,
               /*heartbeat_interval=*/kDefaultHeartbeatInterval,
               /*minimum_time_in_background=*/kDefaultMinimumTimeInBackground);

  policy()->SetActive(false);

  page_node()->SetType(PageType::kTab);
  // Toggle visibility so that the page node updates its last visibility timing
  // information.
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Advance the time by at least `minimum_time_in_background` +
  // `heartbeat_interval`. If a tab is to be discarded, it will be at this
  // point.
  task_env().FastForwardBy(kDefaultHeartbeatInterval +
                           kDefaultMinimumTimeInBackground);
  // No discard.
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HeuristicMemorySaverPolicyTest, DiscardIfPolicyActive) {
  CreatePolicy(/*pmf_threshold_percent=*/100,
               /*heartbeat_interval=*/kDefaultHeartbeatInterval,
               /*minimum_time_in_background=*/kDefaultMinimumTimeInBackground);

  policy()->SetActive(true);

  page_node()->SetType(PageType::kTab);
  // Toggle visibility so that the page node updates its last visibility timing
  // information.
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Advance the time by at least `minimum_time_in_background`
  task_env().FastForwardBy(kDefaultMinimumTimeInBackground);
  // No discard yet.
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Advance by at least the heartbeat interval, this should discard the
  // now-eligible tab.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  task_env().FastForwardBy(kDefaultHeartbeatInterval);

  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HeuristicMemorySaverPolicyTest, NoDiscardIfUnderThreshold) {
  CreatePolicy(/*pmf_threshold_percent=*/30,
               /*heartbeat_interval=*/kDefaultHeartbeatInterval,
               /*minimum_time_in_background=*/kDefaultMinimumTimeInBackground);

  policy()->SetActive(true);

  page_node()->SetType(PageType::kTab);
  // Toggle visibility so that the page node updates its last visibility timing
  // information.
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Advance the time by at least `minimum_time_in_background` +
  // `heartbeat_interval`. If a tab is to be discarded, it will be at this
  // point.
  task_env().FastForwardBy(kDefaultHeartbeatInterval +
                           kDefaultMinimumTimeInBackground);
  // No discard.
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

}  // namespace performance_manager::policies
