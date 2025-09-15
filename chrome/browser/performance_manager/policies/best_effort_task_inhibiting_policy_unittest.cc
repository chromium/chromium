// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/best_effort_task_inhibiting_policy.h"

#include "base/moving_window.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace policies {

class BestEffortTaskInhibitingPolicyTest : public ::testing::Test {
 protected:
  void StartInput() {
    policy_.OnInputScenarioChanged(
        performance_scenarios::ScenarioScope::kGlobal,
        performance_scenarios::InputScenario::kNoInput,
        performance_scenarios::InputScenario::kScroll);
  }

  void EndInput() {
    policy_.OnInputScenarioChanged(
        performance_scenarios::ScenarioScope::kGlobal,
        performance_scenarios::InputScenario::kScroll,
        performance_scenarios::InputScenario::kNoInput);
  }

  void StartNavigation() {
    policy_.OnLoadingScenarioChanged(
        performance_scenarios::ScenarioScope::kGlobal,
        performance_scenarios::LoadingScenario::kNoPageLoading,
        performance_scenarios::LoadingScenario::kFocusedPageLoading);
  }

  void EndNavigation() {
    policy_.OnLoadingScenarioChanged(
        performance_scenarios::ScenarioScope::kGlobal,
        performance_scenarios::LoadingScenario::kFocusedPageLoading,
        performance_scenarios::LoadingScenario::kNoPageLoading);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BestEffortTaskInhibitingPolicy policy_;
};

TEST_F(BestEffortTaskInhibitingPolicyTest, Start) {
  // Right after startup there is no fence.
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());
}

TEST_F(BestEffortTaskInhibitingPolicyTest, RunningQuota) {
  // How much fence time is allowed within a period.
  const base::TimeDelta kMaxFenceTimeBeforeQuotaExceeded =
      policy_.period_duration() - policy_.minimum_duration_without_fence();

  // Fence starts here.
  StartNavigation();
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());

  // Maximum fence time for the period was surpassed so no fence allowed right
  // now.
  task_environment_.FastForwardBy(kMaxFenceTimeBeforeQuotaExceeded);
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());

  // Additional scenarios still do not install fence
  StartInput();
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());

  // After best effort tasks have been allowed for long enough it's possible to
  // install fences again.
  task_environment_.FastForwardBy(policy_.minimum_duration_without_fence());

  // Once removed the fence is not added back automatically.
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());

  // Restarting a scenario does though.
  StartNavigation();
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());
}

TEST_F(BestEffortTaskInhibitingPolicyTest, AdditiveQuota) {
  // How much fence time is allowed within a period.
  base::TimeDelta fence_time_before_quota_exceeded =
      policy_.period_duration() - policy_.minimum_duration_without_fence();

  // Fence starts here.
  StartNavigation();

  base::TimeDelta first_delay = policy_.period_duration() / 2;
  fence_time_before_quota_exceeded -= first_delay;
  task_environment_.FastForwardBy(first_delay);

  // Quota not exceeded yet
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());

  EndNavigation();

  // Navigation ended. No reason to have a fence.
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());

  // Fence activated for the rest of the allowed time.
  StartNavigation();
  task_environment_.FastForwardBy(fence_time_before_quota_exceeded);
  EndNavigation();

  // Quota now exceeded.
  StartNavigation();
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());

  // After best effort tasks have be allowed for long enough it's possible to
  // install fences again.
  task_environment_.FastForwardBy(policy_.minimum_duration_without_fence());

  // Once removed the fence is not added back automatically.
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());

  // Restarting a scenario does though.
  StartNavigation();
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());
}

TEST_F(BestEffortTaskInhibitingPolicyTest, QuotaResetsAfterPeriod) {
  base::TimeDelta fence_time_before_quota_exceeded =
      policy_.period_duration() - policy_.minimum_duration_without_fence();

  base::TimeDelta first_fence_time = fence_time_before_quota_exceeded / 2;
  base::TimeDelta second_fence_time = fence_time_before_quota_exceeded / 3;

  // A face is up for half the maximum.
  StartInput();
  task_environment_.FastForwardBy(first_fence_time);
  EndInput();
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());

  // More time elapses, bringing the end of the period.
  task_environment_.FastForwardBy(policy_.period_duration() - first_fence_time);

  // An additional fence can stay up because the quota reset at the end of the
  // period.
  StartNavigation();
  task_environment_.FastForwardBy(second_fence_time);
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());

  // Reaching the end of the quota once again removes the fence.
  task_environment_.FastForwardBy(fence_time_before_quota_exceeded -
                                  second_fence_time);
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());
}

TEST_F(BestEffortTaskInhibitingPolicyTest, LateTimerOverQuota) {
  StartNavigation();
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());

  // Advance the clock greatly so the period expiry task runs late.
  const base::TimeDelta delay = policy_.period_duration() * 2.2;
  task_environment_.AdvanceClock(delay);

  // Trigger running of late tasks.
  task_environment_.FastForwardBy(base::Nanoseconds(1));

  // No fence allowed because of exceeded quota.
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());

  // Still no fence allowed.
  StartInput();
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());
}

TEST_F(BestEffortTaskInhibitingPolicyTest, LateTimerUnderQuota) {
  task_environment_.FastForwardBy(policy_.period_duration() / 2);

  StartNavigation();
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());

  // Advance the clock greatly so the period expiry task runs late.
  const base::TimeDelta delay = policy_.period_duration();
  task_environment_.AdvanceClock(delay);

  // Trigger running of late tasks.
  task_environment_.FastForwardBy(base::Nanoseconds(1));

  // Fence is allowed because lateness was not sufficient to run over quota
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());
}

TEST_F(BestEffortTaskInhibitingPolicyTest, LateTimerNoFence) {
  // Advance the clock greatly so the period expiry task runs late.
  const base::TimeDelta delay = policy_.period_duration() * 4;
  task_environment_.AdvanceClock(delay);

  // Trigger running of late tasks.
  task_environment_.FastForwardBy(base::Nanoseconds(1));

  // No fence because not needed.
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());
}

// Test that the fence stays up when the loading scenario ends but the input
// scenario continues.
TEST_F(BestEffortTaskInhibitingPolicyTest,
       MultipleScenarios_LoadingEndsInputContinues) {
  StartNavigation();
  StartInput();
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());

  EndNavigation();
  // Fence should remain active due to ongoing input.
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());

  EndInput();
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());
}

// Test that the fence stays up when the input scenario ends but the loading
// scenario continues.
TEST_F(BestEffortTaskInhibitingPolicyTest,
       MultipleScenarios_InputEndsLoadingContinues) {
  StartNavigation();
  StartInput();
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());

  EndInput();
  // Fence should remain active due to ongoing navigation.
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());

  EndNavigation();
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());
}

// This test validates the complex "lateness carry-over" logic.
// 1. A fence is active when the period-end timer should have fired, but the
//    timer is late.
// 2. The lateness is "carried over" as consumed quota in the *new* period.
// 3. This test verifies that this "carried over" time correctly counts against
//    the new period's quota.
TEST_F(BestEffortTaskInhibitingPolicyTest,
       LatenessCarryOver_ExceedsQuotaInNewPeriod) {
  const base::TimeDelta kMaxFenceTime =
      policy_.period_duration() - policy_.minimum_duration_without_fence();
  // Set a lateness that is less than the max fence time, so it triggers
  // carry-over instead of an immediate QuotaExceeded.
  const base::TimeDelta kLateness = kMaxFenceTime / 2;

  StartNavigation();
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());

  // Advance clock to after the period should have ended, plus kLateness.
  task_environment_.AdvanceClock(policy_.period_duration() + kLateness);
  // Trigger the late timer.
  task_environment_.FastForwardBy(base::Nanoseconds(1));

  // Fence should still be up (as in LateTimerUnderQuota).
  // The `kLateness` is now effectively the `cumulative_fence_time_` for the
  // *new* period.
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());

  // Advance the clock by the *remaining* quota time.
  // The lateness from the previous period plus this new time should exactly
  // equal kMaxFenceTime.
  task_environment_.FastForwardBy(kMaxFenceTime - kLateness);

  // The quota for the new period should now be exceeded, triggering
  // QuotaExceeded() and removing the fence.
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());

  // Verify we are in the "no fence" window.
  EndNavigation();
  StartInput();
  EXPECT_FALSE(policy_.IsFenceLiveForTesting());

  // Wait for the "no fence" window to end.
  task_environment_.FastForwardBy(policy_.minimum_duration_without_fence());

  // Fences should be allowed again.
  StartNavigation();
  EXPECT_TRUE(policy_.IsFenceLiveForTesting());
}

}  // namespace policies

}  // namespace performance_manager
