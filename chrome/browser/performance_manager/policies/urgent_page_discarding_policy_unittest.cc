// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/urgent_page_discarding_policy.h"

#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "base/time/time.h"
#include "base/util/memory_pressure/fake_memory_pressure_monitor.h"
#include "chrome/browser/performance_manager/decorators/page_aggregator.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
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
    graph()->TakeFromGraph(policy_);
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

 protected:
  void SimulateMemoryPressure(size_t pressure_event_counts = 1) {
    for (size_t i = 0; i < pressure_event_counts; ++i) {
      mem_pressure_monitor_.SetAndNotifyMemoryPressure(
          base::MemoryPressureListener::MemoryPressureLevel::
              MEMORY_PRESSURE_LEVEL_CRITICAL);
      task_env().RunUntilIdle();
    }
    mem_pressure_monitor_.SetAndNotifyMemoryPressure(
        base::MemoryPressureListener::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_MODERATE);
    task_env().RunUntilIdle();
  }

  util::test::FakeMemoryPressureMonitor* mem_pressure_monitor() {
    return &mem_pressure_monitor_;
  }

 private:
  util::test::FakeMemoryPressureMonitor mem_pressure_monitor_;
  UrgentPageDiscardingPolicy* policy_;
};

TEST_F(UrgentPageDiscardingPolicyTest, DiscardOnCriticalPressure) {
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));

  mem_pressure_monitor()->SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(UrgentPageDiscardingPolicyTest, NoDiscardOnModeratePressure) {
  // No tab should be discarded on moderate pressure.
  mem_pressure_monitor()->SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_MODERATE);
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
}

}  // namespace policies
}  // namespace performance_manager
