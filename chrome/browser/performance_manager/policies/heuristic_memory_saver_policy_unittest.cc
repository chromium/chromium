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

const uint64_t kBytesPerGb = 1024 * 1024 * 1024;

const base::TimeDelta kDefaultHeartbeatInterval = base::Seconds(10);
const base::TimeDelta kLongHeartbeatInterval = base::Minutes(1);
const base::TimeDelta kDefaultMinimumTimeInBackground = base::Seconds(11);

class MemoryMetricsMocker {
 public:
  uint64_t GetAvailableMemory() {
    ++available_memory_sampled_count;
    return available_memory_;
  }

  uint64_t GetTotalMemory() { return total_memory_; }

  void SetAvailableMemory(uint64_t available_memory) {
    available_memory_ = available_memory;
  }

  void SetTotalMemory(uint64_t total_memory) { total_memory_ = total_memory; }

  int available_memory_sampled_count = 0;

 private:
  uint64_t available_memory_ = 0;
  uint64_t total_memory_ = 0;
};

class HeuristicMemorySaverPolicyTest
    : public testing::GraphTestHarnessWithMockDiscarder {
 protected:
  void SetUp() override {
    testing::GraphTestHarnessWithMockDiscarder::SetUp();

    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {performance_manager::features::kHeuristicMemorySaver},
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
      uint64_t pmf_threshold_mb,
      base::TimeDelta threshold_reached_heartbeat_interval,
      base::TimeDelta threshold_not_reached_heartbeat_interval,
      base::TimeDelta minimum_time_in_background,
      HeuristicMemorySaverPolicy::AvailableMemoryCallback
          available_memory_callback = base::BindRepeating([]() {
            return kDefaultAvailableMemoryValue;
          }),
      HeuristicMemorySaverPolicy::TotalMemoryCallback total_memory_callback =
          base::BindRepeating([]() { return kDefaultTotalMemoryValue; })) {
    auto policy = std::make_unique<HeuristicMemorySaverPolicy>(
        pmf_threshold_percent, pmf_threshold_mb,
        threshold_reached_heartbeat_interval,
        threshold_not_reached_heartbeat_interval, minimum_time_in_background,
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
  CreatePolicy(
      /*pmf_threshold_percent=*/100,
      /*pmf_threshold_mb=*/100,
      /*threshold_reached_heartbeat_interval=*/kDefaultHeartbeatInterval,
      /*threshold_not_reached_heartbeat_interval=*/kDefaultHeartbeatInterval,
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
  CreatePolicy(
      /*pmf_threshold_percent=*/100,
      /*pmf_threshold_mb=*/100,
      /*threshold_reached_heartbeat_interval=*/kDefaultHeartbeatInterval,
      /*threshold_not_reached_heartbeat_interval=*/kDefaultHeartbeatInterval,
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

TEST_F(HeuristicMemorySaverPolicyTest, NoDiscardIfAboveThreshold) {
  CreatePolicy(
      /*pmf_threshold_percent=*/30,
      /*pmf_threshold_mb=*/100,
      /*threshold_reached_heartbeat_interval=*/kDefaultHeartbeatInterval,
      /*threshold_not_reached_heartbeat_interval=*/kDefaultHeartbeatInterval,
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

// Tests the case used as an example in the
// `kHeuristicMemorySaverAvailableMemoryThresholdPercent` comment:
//
// A device with 8Gb of installed RAM, 1Gb of which is available is under the
// threshold and will discard tabs (12.5% available and 1Gb < 2048Mb)
TEST_F(HeuristicMemorySaverPolicyTest,
       DiscardIfPolicyActiveAndUnderBothThresholds) {
  MemoryMetricsMocker mocker;
  mocker.SetAvailableMemory(1 * kBytesPerGb);
  mocker.SetTotalMemory(8 * kBytesPerGb);

  CreatePolicy(
      /*pmf_threshold_percent=*/20,
      /*pmf_threshold_mb=*/2048,
      /*threshold_reached_heartbeat_interval=*/kDefaultHeartbeatInterval,
      /*threshold_not_reached_heartbeat_interval=*/kDefaultHeartbeatInterval,
      /*minimum_time_in_background=*/kDefaultMinimumTimeInBackground,
      /*available_memory_callback=*/
      base::BindRepeating(&MemoryMetricsMocker::GetAvailableMemory,
                          base::Unretained(&mocker)),
      /*total_memory_callback=*/
      base::BindRepeating(&MemoryMetricsMocker::GetTotalMemory,
                          base::Unretained(&mocker)));

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

// Tests the case used as an example in the
// `kHeuristicMemorySaverAvailableMemoryThresholdPercent` comment:
//
// A device with 16Gb of installed RAM, 3Gb of which are available is under
// the percentage threshold but will not discard tabs because it's above the
// absolute Mb threshold (18.75% available, but 3Gb > 2048Mb)
TEST_F(HeuristicMemorySaverPolicyTest,
       NoDiscardIfUnderPercentThresholdAboveMbThreshold) {
  MemoryMetricsMocker mocker;
  mocker.SetAvailableMemory(3 * kBytesPerGb);
  mocker.SetTotalMemory(16 * kBytesPerGb);

  CreatePolicy(
      /*pmf_threshold_percent=*/20,
      /*pmf_threshold_mb=*/2048,
      /*threshold_reached_heartbeat_interval=*/kDefaultHeartbeatInterval,
      /*threshold_not_reached_heartbeat_interval=*/kDefaultHeartbeatInterval,
      /*minimum_time_in_background=*/kDefaultMinimumTimeInBackground,
      /*available_memory_callback=*/
      base::BindRepeating(&MemoryMetricsMocker::GetAvailableMemory,
                          base::Unretained(&mocker)),
      /*total_memory_callback=*/
      base::BindRepeating(&MemoryMetricsMocker::GetTotalMemory,
                          base::Unretained(&mocker)));

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

TEST_F(HeuristicMemorySaverPolicyTest, DifferentThresholds) {
  MemoryMetricsMocker mocker;
  mocker.SetAvailableMemory(60);
  mocker.SetTotalMemory(100);

  CreatePolicy(
      /*pmf_threshold_percent=*/30,
      /*pmf_threshold_mb=*/100,
      /*threshold_reached_heartbeat_interval=*/kDefaultHeartbeatInterval,
      /*threshold_not_reached_heartbeat_interval=*/kLongHeartbeatInterval,
      /*minimum_time_in_background=*/kDefaultMinimumTimeInBackground,
      /*available_memory_callback=*/
      base::BindRepeating(&MemoryMetricsMocker::GetAvailableMemory,
                          base::Unretained(&mocker)),
      /*total_memory_callback=*/
      base::BindRepeating(&MemoryMetricsMocker::GetTotalMemory,
                          base::Unretained(&mocker)));

  policy()->SetActive(true);

  // Advance the time just enough to get the first heartbeat
  task_env().FastForwardBy(kDefaultHeartbeatInterval);
  // No discard yet.
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  // The memory was sampled once in the callback.
  EXPECT_EQ(1, mocker.available_memory_sampled_count);

  // Simulate reaching the threshold so that the next heartbeat callback
  // schedules the timer with the short interval.
  mocker.SetAvailableMemory(10);
  mocker.SetTotalMemory(100);

  // Advance the time by one short heartbeat again. Memory shouldn't be sampled
  // a second time because the next heartbeat was scheduled for the long
  // interval (since we weren't past the threshold).
  task_env().FastForwardBy(kDefaultHeartbeatInterval);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  EXPECT_EQ(1, mocker.available_memory_sampled_count);

  // Advance by the difference between the long and short heartbeats, so that we
  // just reach the long one. This should trigger the timer's callback, sample
  // memory and see that we're above the threshold, and discard a tab + schedule
  // the next check using the short interval.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  task_env().FastForwardBy(kLongHeartbeatInterval - kDefaultHeartbeatInterval);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  EXPECT_EQ(2, mocker.available_memory_sampled_count);

  // Simulate that the discard got us back under the threshold.
  mocker.SetAvailableMemory(40);
  mocker.SetTotalMemory(100);

  // After the short interval, the memory is sampled again (and seen under the
  // threshold). No tab is discarded and the next heartbeat is scheduled using
  // the long interval.
  task_env().FastForwardBy(kDefaultHeartbeatInterval);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  EXPECT_EQ(3, mocker.available_memory_sampled_count);

  // Verify that there was no sampling after the short interval but there was
  // one after the long interval.
  task_env().FastForwardBy(kDefaultHeartbeatInterval);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  EXPECT_EQ(3, mocker.available_memory_sampled_count);

  task_env().FastForwardBy(kLongHeartbeatInterval - kDefaultHeartbeatInterval);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  EXPECT_EQ(4, mocker.available_memory_sampled_count);
}

}  // namespace performance_manager::policies
