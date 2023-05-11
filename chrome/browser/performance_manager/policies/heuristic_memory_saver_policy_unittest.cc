// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/heuristic_memory_saver_policy.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/common/page_type.h"

namespace performance_manager::policies {

using GraphTestHarnessWithMockDiscarder =
    testing::GraphTestHarnessWithMockDiscarder;
using ::testing::Return;

const uint64_t kDefaultAvailableMemoryValue = 60;
const uint64_t kDefaultTotalMemoryValue = 100;

const uint64_t kBytesPerGb = 1024 * 1024 * 1024;

const base::TimeDelta kDefaultHeartbeatInterval = base::Seconds(10);
const base::TimeDelta kLongHeartbeatInterval = base::Minutes(1);
const base::TimeDelta kDefaultMinimumTimeInBackground = base::Seconds(11);

std::string FormatTimeDeltaParam(base::TimeDelta delta) {
  return base::StrCat({base::NumberToString(delta.InSeconds()), "s"});
}

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
    : public GraphTestHarnessWithMockDiscarder {
 protected:
  void SetUp() override {
    GraphTestHarnessWithMockDiscarder::SetUp();

    // This is usually called when the profile is created. Fake it here since it
    // doesn't happen in tests.
    PageDiscardingHelper::GetFromGraph(graph())->SetNoDiscardPatternsForProfile(
        page_node()->browser_context_id(), {});
  }

  void TearDown() override {
    graph()->TakeFromGraph(policy_);
    GraphTestHarnessWithMockDiscarder::TearDown();
  }

  // Creates the policy by forwarding it the arguments and passing it to the
  // graph, with a default set of functions for memory measurements that
  // always return 60 and 100.
  void CreatePolicy(
      uint64_t pmf_threshold_percent = 100,
      uint64_t pmf_threshold_mb = 100,
      base::TimeDelta threshold_reached_heartbeat_interval =
          kDefaultHeartbeatInterval,
      base::TimeDelta threshold_not_reached_heartbeat_interval =
          kDefaultHeartbeatInterval,
      base::TimeDelta minimum_time_in_background =
          kDefaultMinimumTimeInBackground,
      bool feature_enabled = true,
      HeuristicMemorySaverPolicy::AvailableMemoryCallback
          available_memory_callback = base::BindRepeating([] {
            return kDefaultAvailableMemoryValue;
          }),
      HeuristicMemorySaverPolicy::TotalMemoryCallback total_memory_callback =
          base::BindRepeating([] { return kDefaultTotalMemoryValue; })) {
    if (feature_enabled) {
      feature_list_.InitAndEnableFeatureWithParameters(
          features::kHeuristicMemorySaver,
          {
              {"threshold_percent",
               base::NumberToString(pmf_threshold_percent)},
              {"threshold_mb", base::NumberToString(pmf_threshold_mb)},
              {"threshold_reached_heartbeat_interval",
               FormatTimeDeltaParam(threshold_reached_heartbeat_interval)},
              {"threshold_not_reached_heartbeat_interval",
               FormatTimeDeltaParam(threshold_not_reached_heartbeat_interval)},
              {"minimum_time_in_background",
               FormatTimeDeltaParam(minimum_time_in_background)},
          });
    } else {
      feature_list_.InitAndDisableFeature(features::kHeuristicMemorySaver);
    }

    auto policy = std::make_unique<HeuristicMemorySaverPolicy>(
        available_memory_callback, total_memory_callback);
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));
  }

  PageNodeImpl* CreateOtherPageNode() {
    other_process_node_ = CreateNode<ProcessNodeImpl>();
    other_page_node_ = CreateNode<PageNodeImpl>();
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

  TestNodeWrapper<PageNodeImpl> other_page_node_;
  TestNodeWrapper<ProcessNodeImpl> other_process_node_;
  TestNodeWrapper<FrameNodeImpl> other_main_frame_node_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(HeuristicMemorySaverPolicyTest, NoDiscardIfPolicyInactive) {
  CreatePolicy();
  policy()->SetActive(false);

  page_node()->SetType(PageType::kTab);
  // Toggle visibility so that the page node updates its last visibility timing
  // information.
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Advance the time by at least `minimum_time_in_background` +
  // `heartbeat_interval`. If a tab is to be discarded, it will be at this
  // point.
  task_env().FastForwardBy(
      policy()->GetThresholdNotReachedHeartbeatIntervalForTesting() +
      policy()->GetMinimumTimeInBackgroundForTesting());
  // No discard.
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HeuristicMemorySaverPolicyTest, DiscardIfPolicyActive) {
  CreatePolicy();
  policy()->SetActive(true);

  page_node()->SetType(PageType::kTab);
  // Toggle visibility so that the page node updates its last visibility timing
  // information.
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Advance the time by at least `minimum_time_in_background`
  task_env().FastForwardBy(policy()->GetMinimumTimeInBackgroundForTesting());
  // No discard yet.
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Advance by at least the heartbeat interval, this should discard the
  // now-eligible tab.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  task_env().FastForwardBy(
      policy()->GetThresholdNotReachedHeartbeatIntervalForTesting());

  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HeuristicMemorySaverPolicyTest, NoDiscardIfAboveThreshold) {
  CreatePolicy(/*pmf_threshold_percent=*/30);
  policy()->SetActive(true);

  page_node()->SetType(PageType::kTab);
  // Toggle visibility so that the page node updates its last visibility timing
  // information.
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Advance the time by at least `minimum_time_in_background` +
  // `heartbeat_interval`. If a tab is to be discarded, it will be at this
  // point.
  task_env().FastForwardBy(
      policy()->GetThresholdNotReachedHeartbeatIntervalForTesting() +
      policy()->GetMinimumTimeInBackgroundForTesting());
  // No discard.
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HeuristicMemorySaverPolicyTest, NoDiscardWithZeroThresholdPercent) {
  // Verify that a 0 threshold doesn't cause division by zero.
  CreatePolicy(/*pmf_threshold_percent=*/0);
  policy()->SetActive(true);

  page_node()->SetType(PageType::kTab);
  // Toggle visibility so that the page node updates its last visibility timing
  // information.
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Advance the time by at least `minimum_time_in_background` +
  // `heartbeat_interval`. If a tab is to be discarded, it will be at this
  // point.
  task_env().FastForwardBy(
      policy()->GetThresholdNotReachedHeartbeatIntervalForTesting() +
      policy()->GetMinimumTimeInBackgroundForTesting());
  // No discard.
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HeuristicMemorySaverPolicyTest, NoDiscardWithZeroThresholdMB) {
  // Verify that a 0 threshold doesn't cause division by zero.
  CreatePolicy(/*pmf_threshold_percent=*/100,
               /*pmf_threshold_mb=*/0);
  policy()->SetActive(true);

  page_node()->SetType(PageType::kTab);
  // Toggle visibility so that the page node updates its last visibility timing
  // information.
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Advance the time by at least `minimum_time_in_background` +
  // `heartbeat_interval`. If a tab is to be discarded, it will be at this
  // point.
  task_env().FastForwardBy(
      policy()->GetThresholdNotReachedHeartbeatIntervalForTesting() +
      policy()->GetMinimumTimeInBackgroundForTesting());
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
      /*feature_enabled=*/true,
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
  task_env().FastForwardBy(policy()->GetMinimumTimeInBackgroundForTesting());
  // No discard yet.
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Advance by at least the heartbeat interval, this should discard the
  // now-eligible tab.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  task_env().FastForwardBy(
      policy()->GetThresholdNotReachedHeartbeatIntervalForTesting());

  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(HeuristicMemorySaverPolicyTest, DiscardIfPolicyActiveWithoutFeature) {
  // Pretend there's 1 byte of memory available out of 8 GB. That should be low
  // enough to trigger a discard no matter what the default thresholds are.
  MemoryMetricsMocker mocker;
  mocker.SetAvailableMemory(1);
  mocker.SetTotalMemory(8 * kBytesPerGb);

  // It should be possible to activate the policy even if the
  // kHeuristicMemorySaver feature is disabled (it just won't happen by
  // default). In this case the feature parameters are ignored.
  CreatePolicy(
      /*pmf_threshold_percent=*/0,
      /*pmf_threshold_mb=*/0,
      /*threshold_reached_heartbeat_interval=*/base::TimeDelta(),
      /*threshold_not_reached_heartbeat_interval=*/base::TimeDelta(),
      /*minimum_time_in_background=*/base::TimeDelta(),
      /*feature_enabled=*/false,
      /*available_memory_callback=*/
      base::BindRepeating(&MemoryMetricsMocker::GetAvailableMemory,
                          base::Unretained(&mocker)),
      /*total_memory_callback=*/
      base::BindRepeating(&MemoryMetricsMocker::GetTotalMemory,
                          base::Unretained(&mocker)));

  policy()->SetActive(true);

  EXPECT_FALSE(
      policy()->GetThresholdReachedHeartbeatIntervalForTesting().is_zero());
  EXPECT_FALSE(
      policy()->GetThresholdNotReachedHeartbeatIntervalForTesting().is_zero());
  EXPECT_FALSE(policy()->GetMinimumTimeInBackgroundForTesting().is_zero());

  page_node()->SetType(PageType::kTab);
  // Toggle visibility so that the page node updates its last visibility timing
  // information.
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Advance the time by at least `minimum_time_in_background` +
  // `heartbeat_interval`. One or more tabs should be discarded by this point.
  // The exact timing depends on the default values of the policy parameters.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillRepeatedly(Return(true));
  task_env().FastForwardBy(
      policy()->GetThresholdNotReachedHeartbeatIntervalForTesting() +
      policy()->GetMinimumTimeInBackgroundForTesting());

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
      /*feature_enabled=*/true,
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
  task_env().FastForwardBy(
      policy()->GetThresholdNotReachedHeartbeatIntervalForTesting() +
      policy()->GetMinimumTimeInBackgroundForTesting());
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
      /*feature_enabled=*/true,
      /*available_memory_callback=*/
      base::BindRepeating(&MemoryMetricsMocker::GetAvailableMemory,
                          base::Unretained(&mocker)),
      /*total_memory_callback=*/
      base::BindRepeating(&MemoryMetricsMocker::GetTotalMemory,
                          base::Unretained(&mocker)));

  policy()->SetActive(true);

  EXPECT_EQ(policy()->GetThresholdNotReachedHeartbeatIntervalForTesting(),
            kLongHeartbeatInterval);
  EXPECT_EQ(policy()->GetThresholdReachedHeartbeatIntervalForTesting(),
            kDefaultHeartbeatInterval);

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
      .WillOnce(Return(true));
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
