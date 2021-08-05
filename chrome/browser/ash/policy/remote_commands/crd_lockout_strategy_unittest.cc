// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_lockout_strategy.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using base::TimeDelta;

class CrdFixedTimeoutLockoutStrategyTest : public ::testing::Test {
 public:
  CrdFixedTimeoutLockoutStrategyTest() = default;
  CrdFixedTimeoutLockoutStrategyTest(
      const CrdFixedTimeoutLockoutStrategyTest&) = delete;
  CrdFixedTimeoutLockoutStrategyTest& operator=(
      const CrdFixedTimeoutLockoutStrategyTest&) = delete;
  ~CrdFixedTimeoutLockoutStrategyTest() override = default;

  CrdFixedTimeoutLockoutStrategy& strategy() { return strategy_; }

  void FastForwardBy(TimeDelta delta) { environment_.FastForwardBy(delta); }

 private:
  CrdFixedTimeoutLockoutStrategy strategy_;

  base::test::SingleThreadTaskEnvironment environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(CrdFixedTimeoutLockoutStrategyTest, ShouldAllowConnectionInitially) {
  EXPECT_TRUE(strategy().CanAttemptConnection());
}

TEST_F(CrdFixedTimeoutLockoutStrategyTest,
       ShouldAllowConnectionsAfterLessThan3RejectedAttempts) {
  EXPECT_TRUE(strategy().CanAttemptConnection());

  strategy().OnConnectionRejected();
  EXPECT_TRUE(strategy().CanAttemptConnection());

  strategy().OnConnectionRejected();
  EXPECT_TRUE(strategy().CanAttemptConnection());
}

TEST_F(CrdFixedTimeoutLockoutStrategyTest,
       ShouldBlockConnectionsAfter3RejectedAttempts) {
  strategy().OnConnectionRejected();
  strategy().OnConnectionRejected();
  strategy().OnConnectionRejected();

  EXPECT_FALSE(strategy().CanAttemptConnection());
}

TEST_F(CrdFixedTimeoutLockoutStrategyTest,
       ShouldBlockConnectionsFor5MinutesAfter3RejectedAttempts) {
  strategy().OnConnectionRejected();
  strategy().OnConnectionRejected();
  strategy().OnConnectionRejected();
  EXPECT_FALSE(strategy().CanAttemptConnection());

  // 4:59 later
  FastForwardBy(TimeDelta::FromMinutes(5) - TimeDelta::FromSeconds(1));
  EXPECT_FALSE(strategy().CanAttemptConnection());

  // 5:01 later
  FastForwardBy(TimeDelta::FromSeconds(2));
  EXPECT_TRUE(strategy().CanAttemptConnection());
}

TEST_F(CrdFixedTimeoutLockoutStrategyTest,
       SuccesfullConnectionShouldResetFailedConnectionCount) {
  strategy().OnConnectionRejected();
  strategy().OnConnectionRejected();
  strategy().OnConnectionEstablished();
  strategy().OnConnectionRejected();
  strategy().OnConnectionRejected();

  EXPECT_TRUE(strategy().CanAttemptConnection());
}

TEST_F(CrdFixedTimeoutLockoutStrategyTest, ShouldAlsoCountOldRejectedAttempts) {
  strategy().OnConnectionRejected();
  FastForwardBy(TimeDelta::FromHours(1));

  strategy().OnConnectionRejected();
  FastForwardBy(TimeDelta::FromHours(1));

  strategy().OnConnectionRejected();

  EXPECT_FALSE(strategy().CanAttemptConnection());
}

TEST_F(CrdFixedTimeoutLockoutStrategyTest,
       ShouldResetRejectedAttemptCountAfterLockoutExpires) {
  // Fail 3 times
  strategy().OnConnectionRejected();
  strategy().OnConnectionRejected();
  strategy().OnConnectionRejected();

  // Wait for timeout
  FastForwardBy(TimeDelta::FromMinutes(5) + TimeDelta::FromSeconds(1));
  EXPECT_TRUE(strategy().CanAttemptConnection());

  // Fail 2 more times
  strategy().OnConnectionRejected();
  strategy().OnConnectionRejected();

  EXPECT_TRUE(strategy().CanAttemptConnection());

  // Fail 1 more time
  strategy().OnConnectionRejected();
  EXPECT_FALSE(strategy().CanAttemptConnection());
}

}  // namespace policy
