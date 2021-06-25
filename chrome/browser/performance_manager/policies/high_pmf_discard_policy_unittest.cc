// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/high_pmf_discard_policy.h"

#include "base/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/util/memory_pressure/fake_memory_pressure_monitor.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

const int kPMFLimitKb = 100 * 1024;

class HighPMFDiscardPolicyTest
    : public testing::GraphTestHarnessWithMockDiscarder {
 public:
  HighPMFDiscardPolicyTest() = default;
  ~HighPMFDiscardPolicyTest() override = default;
  HighPMFDiscardPolicyTest(const HighPMFDiscardPolicyTest& other) = delete;
  HighPMFDiscardPolicyTest& operator=(const HighPMFDiscardPolicyTest&) = delete;

  void SetUp() override {
    testing::GraphTestHarnessWithMockDiscarder::SetUp();

    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<HighPMFDiscardPolicy>();
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    histogram_tester_.reset();
    graph()->TakeFromGraph(policy_);
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

 protected:
  HighPMFDiscardPolicy* policy() { return policy_; }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  util::test::FakeMemoryPressureMonitor& memory_pressure_monitor() {
    return memory_pressure_monitor_;
  }

 private:
  HighPMFDiscardPolicy* policy_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  util::test::FakeMemoryPressureMonitor memory_pressure_monitor_;
};

TEST_F(HighPMFDiscardPolicyTest, EndToEnd) {
  policy()->set_pmf_limit_for_testing(kPMFLimitKb);

  process_node()->set_private_footprint_kb(kPMFLimitKb - 1);
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  // Make sure that no task get posted to the discarder.
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // A call to OnProcessMemoryMetricsAvailable should notify the policy that the
  // total PMF is too high and a tab should be discarded.
  process_node()->set_private_footprint_kb(kPMFLimitKb);
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // Lower the PMF of process_node, this is equivalent to discarding a page and
  // lowering the total PMF.
  process_node()->set_private_footprint_kb(kPMFLimitKb - 1);
  // Call OnProcessMemoryMetricsAvailable to record the post discard metrics.
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  histogram_tester()->ExpectUniqueSample(
      "Discarding.HighPMFPolicy.DiscardAttemptsCount", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Discarding.HighPMFPolicy.SuccessfulDiscardsCount", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Discarding.HighPMFPolicy.DiscardSuccess", true, 1);
  histogram_tester()->ExpectUniqueSample(
      "Discarding.HighPMFPolicy.MemoryReclaimedKbAfterDiscardingATab", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Discarding.HighPMFPolicy.MemoryPressureLevel",

      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE, 1);
}

TEST_F(HighPMFDiscardPolicyTest, Histograms) {
  policy()->set_pmf_limit_for_testing(kPMFLimitKb);
  process_node()->set_private_footprint_kb(kPMFLimitKb);

  // Pretend that the discard attempt has failed.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(false));
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // There's been one unsuccessful discard attempt so far. The total PMF is
  // still higher than the limit and so the number of discards shouldn't be
  // reported.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  PageDiscardingHelper::RemovesDiscardAttemptMarkerForTesting(page_node());
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  histogram_tester()->ExpectTotalCount(
      "Discarding.HighPMFPolicy.DiscardAttemptsCount", 0);
  histogram_tester()->ExpectTotalCount(
      "Discarding.HighPMFPolicy.SuccessfulDiscardsCount", 0);
  histogram_tester()->ExpectTotalCount(
      "Discarding.HighPMFPolicy.MemoryReclaimedKbAfterDiscardingATab", 0);
  histogram_tester()->ExpectUniqueSample(
      "Discarding.HighPMFPolicy.DiscardSuccess", false, 1);

  // Do a successful discard but leave the total PMF as is.
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  // There's been one unsuccessful discard attempt and one successful one so
  // far but the total PMF is still above the limit.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  PageDiscardingHelper::RemovesDiscardAttemptMarkerForTesting(page_node());
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  histogram_tester()->ExpectTotalCount(
      "Discarding.HighPMFPolicy.DiscardAttemptsCount", 0);
  histogram_tester()->ExpectTotalCount(
      "Discarding.HighPMFPolicy.SuccessfulDiscardsCount", 0);
  histogram_tester()->ExpectUniqueSample(
      "Discarding.HighPMFPolicy.MemoryReclaimedKbAfterDiscardingATab", 0, 1);
  histogram_tester()->ExpectBucketCount(
      "Discarding.HighPMFPolicy.DiscardSuccess", false, 1);
  histogram_tester()->ExpectBucketCount(
      "Discarding.HighPMFPolicy.DiscardSuccess", true, 1);

  process_node()->set_private_footprint_kb(kPMFLimitKb - 1);
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();

  histogram_tester()->ExpectUniqueSample(
      "Discarding.HighPMFPolicy.DiscardAttemptsCount", 3, 1);
  histogram_tester()->ExpectUniqueSample(
      "Discarding.HighPMFPolicy.SuccessfulDiscardsCount", 2, 1);
  histogram_tester()->ExpectBucketCount(
      "Discarding.HighPMFPolicy.MemoryReclaimedKbAfterDiscardingATab", 1, 1);
  histogram_tester()->ExpectBucketCount(
      "Discarding.HighPMFPolicy.DiscardSuccess", false, 1);
  histogram_tester()->ExpectBucketCount(
      "Discarding.HighPMFPolicy.DiscardSuccess", true, 2);
}

TEST_F(HighPMFDiscardPolicyTest, NegativeMemoryReclaimGoesInUnderflowBucket) {
  policy()->set_pmf_limit_for_testing(kPMFLimitKb);
  process_node()->set_private_footprint_kb(kPMFLimitKb);

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(true));
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  ::testing::Mock::VerifyAndClearExpectations(discarder());
  process_node()->set_private_footprint_kb(
      process_node()->private_footprint_kb() + 1);

  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();

  // The total PMF has increased, the memory reclaimed should be reported in the
  // underflow bucket.
  histogram_tester()->ExpectUniqueSample(
      "Discarding.HighPMFPolicy.MemoryReclaimedKbAfterDiscardingATab", 0, 1);
}

TEST_F(HighPMFDiscardPolicyTest, MemoryPressureHistograms) {
  policy()->set_pmf_limit_for_testing(kPMFLimitKb);
  process_node()->set_private_footprint_kb(kPMFLimitKb);

  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            memory_pressure_monitor().GetCurrentPressureLevel());

  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(::testing::Return(false));
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  task_env().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(discarder());

  histogram_tester()->ExpectUniqueSample(
      "Discarding.HighPMFPolicy.MemoryPressureLevel",

      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE, 1);

  // Test with MEMORY_PRESSURE_LEVEL_MODERATE.
  memory_pressure_monitor().SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  task_env().RunUntilIdle();

  histogram_tester()->ExpectBucketCount(
      "Discarding.HighPMFPolicy.MemoryPressureLevel",

      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE, 1);

  // Test with MEMORY_PRESSURE_LEVEL_CRITICAL.
  memory_pressure_monitor().SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  graph()->FindOrCreateSystemNodeImpl()->OnProcessMemoryMetricsAvailable();
  task_env().RunUntilIdle();

  histogram_tester()->ExpectBucketCount(
      "Discarding.HighPMFPolicy.MemoryPressureLevel",

      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL, 1);
}

}  // namespace policies
}  // namespace performance_manager
