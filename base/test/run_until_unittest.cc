// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
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
  // happens, so we use EXPECT_NONFATAL_FAILURE to handle this failure.
  // EXPECT_NONFATAL_FAILURE only works on static objects.
  static bool success;

  EXPECT_NONFATAL_FAILURE(
      { success = RunUntil([] { return false; }); }, "timed out");

  EXPECT_FALSE(success);
}

// Tests that RunUntil supports MOCK_TIME when used with a delayed task posted
// directly to the main thread. This verifies that time advances correctly and
// the condition is satisfied after the expected delay.
TEST(RunUntilTestWithThreadPool, SupportsMockTime) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::TimeTicks start_time = base::TimeTicks::Now();
  bool done;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce([](bool* flag) { *flag = true; }, &done),
      base::Days(1));

  EXPECT_TRUE(base::test::RunUntil([&]() { return done; }));

  EXPECT_EQ(base::TimeTicks::Now() - start_time, base::Days(1));
}

// Documents that this API can be flaky if the condition is global (time, global
// var, etc.) and doesn't result in waking the main thread.
TEST(RunUntilTestWithThreadPool, TimesOutWhenMainThreadSleepsForever) {
  TaskEnvironment task_environment;

  base::AtomicFlag done;
  const auto start_time = TimeTicks::Now();

  ThreadPool::PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&]() { done.Set(); }), Milliseconds(1));

  // Program a timeout wakeup on the main thread, it doesn't need to do
  // anything, being awake will cause the RunUntil predicate to be checked once
  // idle again.
  PostDelayedTask(base::DoNothing(), TestTimeouts::tiny_timeout());

  EXPECT_TRUE(RunUntil([&]() { return done.IsSet(); }));

  // Reached timeout on main thread despite condition being satisfied on
  // ThreadPool earlier... Ideally we could flip this expectation to EXPECT_LT
  // but for now it documents the reality.
  auto wait_time = TimeTicks::Now() - start_time;

  // TODO(crbug.com/368805258): The main thread on iOS seems to wakeup without
  // waiting for the delayed task to fire, causing the condition to be checked
  // early, unexpectedly.
#if !BUILDFLAG(IS_IOS)
  EXPECT_GE(wait_time, TestTimeouts::tiny_timeout());
#else
  // Just check if RunUntil did it's job.
  EXPECT_GE(wait_time, Milliseconds(1));
#endif
  EXPECT_TRUE(done.IsSet());
}

// Same as "TimesOutWhenMainThreadSleepsForever" but under MOCK_TIME.
// We would similarly like this to exit RunUntil after the condition is
// satisfied after 1ms but this documents that this is not currently WAI.
TEST(RunUntilTestWithMockTime, ConditionOnlyObservedIfWorkIsDone) {
  TaskEnvironment task_environment{TaskEnvironment::TimeSource::MOCK_TIME};

  base::AtomicFlag done;
  const auto start_time = TimeTicks::Now();

  ThreadPool::PostDelayedTask(FROM_HERE,
                              BindLambdaForTesting([&done]() { done.Set(); }),
                              Milliseconds(1));
  PostDelayedTask(base::DoNothing(), TestTimeouts::tiny_timeout());
  EXPECT_TRUE(RunUntil([&]() { return done.IsSet(); }));
  // Should be exactly EQ under MOCK_TIME.
  EXPECT_EQ(TimeTicks::Now() - start_time, TestTimeouts::tiny_timeout());
}

}  // namespace base::test
