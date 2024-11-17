// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/urgent_page_discarding_policy.h"

#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "components/memory_pressure/fake_memory_pressure_monitor.h"
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

    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<UrgentPageDiscardingPolicy>();
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));
  }

  void TearDown() override {
    policy_ = nullptr;
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

 private:
  raw_ptr<UrgentPageDiscardingPolicy> policy_ = nullptr;
};

TEST_F(UrgentPageDiscardingPolicyTest, DiscardOnCriticalPressure) {
  base::RunLoop run_loop;
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(
          ::testing::DoAll(::testing::Invoke(&run_loop, &base::RunLoop::Quit),
                           ::testing::Return(true)));
  system_node()->OnMemoryPressureForTesting(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL);
  run_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Send a second memory pressure notification without switching back to the
  // no pressure state. This happens when a single discard isn't sufficient to
  // exit memory pressure.
  base::RunLoop run_loop2;
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(
          ::testing::DoAll(::testing::Invoke(&run_loop2, &base::RunLoop::Quit),
                           ::testing::Return(true)));
  PageDiscardingHelper::RemovesDiscardAttemptMarkerForTesting(page_node());
  system_node()->OnMemoryPressureForTesting(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL);
  run_loop2.Run();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(UrgentPageDiscardingPolicyTest, NoDiscardOnModeratePressure) {
  // No tab should be discarded on moderate pressure.
  system_node()->OnMemoryPressureForTesting(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_MODERATE);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

}  // namespace policies
}  // namespace performance_manager
