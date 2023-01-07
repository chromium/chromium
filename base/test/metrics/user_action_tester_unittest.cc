// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/user_action_tester.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::UnorderedElementsAre;

namespace base {

namespace {

const char kUserAction1[] = "user.action.1";
const char kUserAction2[] = "user.action.2";
const char kUserAction3[] = "user.action.3";

// Record an action and cause all ActionCallback observers to be notified.
void RecordAction(const char user_action[]) {
  base::RecordAction(base::UserMetricsAction(user_action));
}

}  // namespace

// Verify user action counts are zero initially.
TEST(UserActionTesterTest, GetActionCountWhenNoActionsHaveBeenRecorded) {
  UserActionTester user_action_tester;
  EXPECT_EQ(0, user_action_tester.GetActionCount(kUserAction1));
}

// Verify user action counts are zero initially.
TEST(UserActionTesterTest, GetActionTimesWhenNoActionsHaveBeenRecorded) {
  UserActionTester user_action_tester;
  EXPECT_TRUE(user_action_tester.GetActionTimes(kUserAction1).empty());
}

// Verify user action counts are tracked properly.
TEST(UserActionTesterTest, GetActionCountWhenActionsHaveBeenRecorded) {
  UserActionTester user_action_tester;

  RecordAction(kUserAction1);
  RecordAction(kUserAction2);
  RecordAction(kUserAction2);

  EXPECT_EQ(1, user_action_tester.GetActionCount(kUserAction1));
  EXPECT_EQ(2, user_action_tester.GetActionCount(kUserAction2));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kUserAction3));
}

// Verify user action times are tracked properly.
TEST(UserActionTesterTest, GetActionTimesWhenActionsHaveBeenRecorded) {
  ScopedMockClockOverride clock;
  UserActionTester user_action_tester;

  TimeTicks t1 = TimeTicks::Now();
  RecordAction(kUserAction1);
  clock.Advance(Minutes(10));

  TimeTicks t2 = TimeTicks::Now();
  RecordAction(kUserAction2);
  clock.Advance(Minutes(20));

  TimeTicks t3 = TimeTicks::Now();
  RecordAction(kUserAction3);

  EXPECT_THAT(user_action_tester.GetActionTimes(kUserAction1),
              UnorderedElementsAre(t1));
  EXPECT_THAT(user_action_tester.GetActionTimes(kUserAction2),
              UnorderedElementsAre(t2));
  EXPECT_THAT(user_action_tester.GetActionTimes(kUserAction3),
              UnorderedElementsAre(t3));
}

// Verify no seg faults occur when resetting action counts when none have been
// recorded.
TEST(UserActionTesterTest, ResetCountsWhenNoActionsHaveBeenRecorded) {
  UserActionTester user_action_tester;
  user_action_tester.ResetCounts();
}

// Verify user action counts are set to zero on a ResetCounts.
TEST(UserActionTesterTest, ResetCountsWhenActionsHaveBeenRecorded) {
  UserActionTester user_action_tester;

  RecordAction(kUserAction1);
  RecordAction(kUserAction1);
  RecordAction(kUserAction2);
  user_action_tester.ResetCounts();

  EXPECT_EQ(0, user_action_tester.GetActionCount(kUserAction1));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kUserAction2));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kUserAction3));
}

// Verify user action times are cleared on a ResetCounts.
TEST(UserActionTesterTest, ResetTimesWhenActionsHaveBeenRecorded) {
  UserActionTester user_action_tester;

  RecordAction(kUserAction1);
  RecordAction(kUserAction1);
  RecordAction(kUserAction2);
  user_action_tester.ResetCounts();

  EXPECT_TRUE(user_action_tester.GetActionTimes(kUserAction1).empty());
  EXPECT_TRUE(user_action_tester.GetActionTimes(kUserAction2).empty());
  EXPECT_TRUE(user_action_tester.GetActionTimes(kUserAction3).empty());
}

// Verify the UserActionsTester is notified when base::RecordAction is called.
TEST(UserActionTesterTest, VerifyUserActionTesterListensForUserActions) {
  ScopedMockClockOverride clock;
  UserActionTester user_action_tester;

  TimeTicks time = TimeTicks::Now();
  base::RecordAction(base::UserMetricsAction(kUserAction1));

  EXPECT_EQ(1, user_action_tester.GetActionCount(kUserAction1));
  EXPECT_THAT(user_action_tester.GetActionTimes(kUserAction1),
              UnorderedElementsAre(time));
}

// Verify the UserActionsTester is notified when base::RecordComputedAction is
// called.
TEST(UserActionTesterTest,
     VerifyUserActionTesterListensForComputedUserActions) {
  ScopedMockClockOverride clock;
  UserActionTester user_action_tester;

  TimeTicks time = TimeTicks::Now();
  base::RecordComputedAction(kUserAction1);

  EXPECT_EQ(1, user_action_tester.GetActionCount(kUserAction1));
  EXPECT_THAT(user_action_tester.GetActionTimes(kUserAction1),
              UnorderedElementsAre(time));
}

// Verify the UserActionsTester is notified when base::RecordComputedActionAt is
// called.
TEST(UserActionTesterTest,
     VerifyUserActionTesterListensForComputedUserActionAt) {
  UserActionTester user_action_tester;

  TimeTicks time = TimeTicks::Now() - Minutes(10);
  base::RecordComputedActionAt(kUserAction1, time);

  EXPECT_EQ(1, user_action_tester.GetActionCount(kUserAction1));
  EXPECT_THAT(user_action_tester.GetActionTimes(kUserAction1),
              UnorderedElementsAre(time));
}

// Verify the UserActionsTester is notified when base::RecordComputedActionSince
// is called.
TEST(UserActionTesterTest,
     VerifyUserActionTesterListensForComputedUserActionSince) {
  ScopedMockClockOverride clock;
  UserActionTester user_action_tester;

  TimeTicks time = TimeTicks::Now();
  base::RecordComputedActionSince(kUserAction1, Minutes(20));
  TimeTicks expected_time = time - Minutes(20);

  EXPECT_EQ(1, user_action_tester.GetActionCount(kUserAction1));
  EXPECT_THAT(user_action_tester.GetActionTimes(kUserAction1),
              UnorderedElementsAre(expected_time));
}

}  // namespace base
