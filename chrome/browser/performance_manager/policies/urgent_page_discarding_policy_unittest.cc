// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/urgent_page_discarding_policy.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/memory_coordinator/utils.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "components/performance_manager/decorators/page_aggregator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

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
  void TriggerMemoryPressure(int memory_limit) {
    test_memory_consumer_registry_.NotifyUpdateMemoryLimitAsync(
        memory_limit, base::DoNothing());
    test_memory_consumer_registry_.NotifyReleaseMemoryAsync(
        task_env().QuitClosure());
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
  TriggerMemoryPressure(base::kCriticalMemoryPressureThreshold);
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Send a second memory pressure notification without switching back to the
  // no pressure state. This happens when a single discard isn't sufficient to
  // exit memory pressure.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  DiscardEligibilityPolicy::RemovesDiscardAttemptMarkerForTesting(page_node());
  TriggerMemoryPressure(base::kCriticalMemoryPressureThreshold);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(UrgentPageDiscardingPolicyTest, NoDiscardOnModeratePressure) {
  // No tab should be discarded on moderate pressure.
  TriggerMemoryPressure(base::kModerateMemoryPressureThreshold);
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

}  // namespace policies
}  // namespace performance_manager
