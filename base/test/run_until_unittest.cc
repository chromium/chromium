// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

namespace {

template <typename Lambda>
void RunLater(Lambda lambda) {
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(lambda));
}

void PostDelayedTask(base::OnceClosure closure, base::TimeDelta delay) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(closure), delay);
}

}  // namespace

class RunUntilTest : public ::testing::Test {
 public:
  RunUntilTest() = default;
  RunUntilTest(const RunUntilTest&) = delete;
  RunUntilTest& operator=(const RunUntilTest&) = delete;
  ~RunUntilTest() override = default;

 private:
  test::SingleThreadTaskEnvironment environment_;
};

TEST_F(RunUntilTest, ShouldReturnTrueIfPredicateIsAlreadyFulfilled) {
  EXPECT_TRUE(RunUntil([] { return true; }));
}

TEST_F(RunUntilTest, ShouldReturnTrueOncePredicateIsFulfilled) {
  bool done = false;

  RunLater([&done] { done = true; });

  EXPECT_TRUE(RunUntil([&done] { return done; }));
}

TEST_F(RunUntilTest, ShouldNotSimplyActivelyInvokePredicateInALoop) {
  bool done = false;
  int call_count = 0;

  PostDelayedTask(base::BindLambdaForTesting([&done] { done = true; }),
                  base::Milliseconds(50));

  EXPECT_TRUE(RunUntil([&] {
    call_count++;
    return done;
  }));

  // Ensure the predicate is not called a ton of times.
  EXPECT_LT(call_count, 10);
}

TEST_F(RunUntilTest, ShouldNotSimplyReturnOnFirstIdle) {
  bool done = false;

  PostDelayedTask(base::DoNothing(), base::Milliseconds(1));
  PostDelayedTask(base::DoNothing(), base::Milliseconds(5));
  PostDelayedTask(base::BindLambdaForTesting([&done] { done = true; }),
                  base::Milliseconds(10));

  EXPECT_TRUE(RunUntil([&] { return done; }));
}

TEST_F(RunUntilTest,
       ShouldAlwaysLetOtherTasksRunFirstEvenIfPredicateIsAlreadyFulfilled) {
  // This ensures that no tests can (accidentally) rely on `RunUntil`
  // immediately returning.
  bool other_job_done = false;
  RunLater([&other_job_done] { other_job_done = true; });

  EXPECT_TRUE(RunUntil([] { return true; }));

  EXPECT_TRUE(other_job_done);
}

TEST_F(RunUntilTest, ShouldWorkEvenWhenTimerIsRunning) {
  bool done = false;

  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::Seconds(1), base::DoNothing());

  PostDelayedTask(base::BindLambdaForTesting([&done] { done = true; }),
                  base::Milliseconds(10));

  EXPECT_TRUE(RunUntil([&] { return done; }));
}

TEST_F(RunUntilTest, ShouldReturnFalseIfTimeoutHappens) {
  test::ScopedRunLoopTimeout timeout(FROM_HERE, Milliseconds(1));

  // `ScopedRunLoopTimeout` will automatically fail the test when a timeout
  // happens, so we use EXPECT_FATAL_FAILURE to handle this failure.
  // EXPECT_FATAL_FAILURE only works on static objects.
  static bool success;

  EXPECT_NONFATAL_FAILURE(
      { success = RunUntil([] { return false; }); }, "timed out");

  EXPECT_FALSE(success);
}

}  // namespace base::test
