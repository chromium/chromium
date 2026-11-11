// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/urgent_page_discarding_policy.h"

#include <memory>
#include <optional>

#include "base/memory/available_memory_monitor.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/memory_coordinator/utils.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "components/performance_manager/decorators/page_aggregator.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

#if BUILDFLAG(IS_WIN)
class UrgentPageDiscardingPolicyMemoryMonitorTest : public GraphTestHarness {
 public:
  UrgentPageDiscardingPolicyMemoryMonitorTest()
      : GraphTestHarness(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatures(
        {performance_manager::features::kDiscardOnCommitLimit},
        {features::kSustainedPMUrgentDiscarding});
  }

  void SetUp() override {
    GraphTestHarness::SetUp();
    graph()->PassToGraph(std::make_unique<PageDiscardingHelper>());
  }

 protected:
  class CountingMemoryMonitor : public base::AvailableMemoryMonitor {
   public:
    ~CountingMemoryMonitor() override = default;
    int poll_count() const { return poll_count_; }

   private:
    std::optional<MemorySample> ComputeAvailableMemory() override {
      ++poll_count_;
      return std::nullopt;
    }

    int poll_count_ = 0;
  };

  std::unique_ptr<UrgentPageDiscardingPolicy> CreatePolicy() {
    return std::make_unique<UrgentPageDiscardingPolicy>(&monitor_);
  }

  base::TestMemoryConsumerRegistry memory_consumer_registry_;
  CountingMemoryMonitor monitor_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(UrgentPageDiscardingPolicyMemoryMonitorTest, MonitorFollowsPageDemand) {
  auto* policy = graph()->PassToGraph(CreatePolicy());
  task_env().FastForwardBy(base::Seconds(4));
  EXPECT_EQ(0, monitor_.poll_count());

  auto first_page = CreateNode<PageNodeImpl>();
  auto second_page = CreateNode<PageNodeImpl>();
  task_env().FastForwardBy(base::Seconds(2));
  EXPECT_EQ(1, monitor_.poll_count());
  first_page.reset();
  task_env().FastForwardBy(base::Seconds(2));
  EXPECT_EQ(2, monitor_.poll_count());
  second_page.reset();
  task_env().FastForwardBy(base::Seconds(4));
  EXPECT_EQ(2, monitor_.poll_count());

  auto resumed_page = CreateNode<PageNodeImpl>();
  task_env().FastForwardBy(base::Seconds(2));
  EXPECT_EQ(3, monitor_.poll_count());
  auto owned_policy = graph()->TakeFromGraphAs(policy);
  task_env().FastForwardBy(base::Seconds(4));
  EXPECT_EQ(3, monitor_.poll_count());

  // Reattaching to a graph that already has pages must resume monitoring.
  graph()->PassToGraph(std::move(owned_policy));
  task_env().FastForwardBy(base::Seconds(2));
  EXPECT_EQ(4, monitor_.poll_count());
  owned_policy = graph()->TakeFromGraphAs(policy);
}

TEST_F(UrgentPageDiscardingPolicyMemoryMonitorTest,
       OtherObserversKeepMonitoring) {
  class OtherObserver : public base::AvailableMemoryMonitor::Observer {
   public:
    void OnAvailableMemoryUpdated(
        const base::AvailableMemoryMonitor::MemorySample& sample) override {}
  } observer;
  base::ScopedObservation<base::AvailableMemoryMonitor,
                          base::AvailableMemoryMonitor::Observer>
      observation{&observer};
  observation.Observe(&monitor_);
  auto* policy = graph()->PassToGraph(CreatePolicy());
  auto page = CreateNode<PageNodeImpl>();
  task_env().FastForwardBy(base::Seconds(2));
  EXPECT_EQ(1, monitor_.poll_count());

  page.reset();
  task_env().FastForwardBy(base::Seconds(4));
  EXPECT_EQ(3, monitor_.poll_count());
  observation.Reset();
  task_env().FastForwardBy(base::Seconds(4));
  EXPECT_EQ(3, monitor_.poll_count());
  auto owned_policy = graph()->TakeFromGraphAs(policy);
}
#endif  // BUILDFLAG(IS_WIN)

class UrgentPageDiscardingPolicyTest
    : public testing::GraphTestHarnessWithMockDiscarder {
 public:
  UrgentPageDiscardingPolicyTest() = default;
  ~UrgentPageDiscardingPolicyTest() override = default;
  UrgentPageDiscardingPolicyTest(const UrgentPageDiscardingPolicyTest& other) =
      delete;
  UrgentPageDiscardingPolicyTest& operator=(
      const UrgentPageDiscardingPolicyTest&) = delete;

  void SetUp() override {
    testing::GraphTestHarnessWithMockDiscarder::SetUp();

    scoped_feature_list_.InitAndDisableFeature(
        features::kSustainedPMUrgentDiscarding);

    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<UrgentPageDiscardingPolicy>();
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));
  }

  void TearDown() override {
    if (policy_) {
      std::unique_ptr<UrgentPageDiscardingPolicy> policy =
          graph()->TakeFromGraphAs(policy_.get());
      policy_ = nullptr;
      policy.reset();
    }
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

 protected:
  void TriggerMemoryPressure(base::MemoryLimit memory_limit) {
    test_memory_consumer_registry_.NotifyUpdateMemoryLimitAndReleaseAsync(
        memory_limit, task_env().QuitClosure());
    task_env().RunUntilQuit();
  }

  base::TestMemoryConsumerRegistry test_memory_consumer_registry_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<UrgentPageDiscardingPolicy> policy_ = nullptr;
};

TEST_F(UrgentPageDiscardingPolicyTest, DiscardOnCriticalPressure) {
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  TriggerMemoryPressure(base::MemoryLimit::CriticalPressureThreshold());
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Send a second memory pressure notification without switching back to the
  // no pressure state. This happens when a single discard isn't sufficient to
  // exit memory pressure.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  DiscardEligibilityPolicy::RemovesDiscardAttemptMarkerForTesting(page_node());
  TriggerMemoryPressure(base::MemoryLimit::CriticalPressureThreshold());
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(UrgentPageDiscardingPolicyTest, NoDiscardOnModeratePressure) {
  // No tab should be discarded on moderate pressure.
  TriggerMemoryPressure(base::MemoryLimit::ModeratePressureThreshold());
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

}  // namespace policies
}  // namespace performance_manager
