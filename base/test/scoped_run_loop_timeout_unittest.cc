// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_run_loop_timeout.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

TEST(ScopedRunLoopTimeoutTest, TimesOut) {
  TaskEnvironment task_environment;
  RunLoop run_loop;

  static constexpr auto kArbitraryTimeout = Milliseconds(10);
  ScopedRunLoopTimeout run_timeout(FROM_HERE, kArbitraryTimeout);

  // Since the delayed task will be posted only after the message pump starts
  // running, the ScopedRunLoopTimeout will already have started to elapse,
  // so if Run() exits at the correct time then our delayed task will not run.
  SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(IgnoreResult(&SequencedTaskRunner::PostDelayedTask),
               SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
               MakeExpectedNotRunClosure(FROM_HERE), kArbitraryTimeout));

  // This task should get to run before Run() times-out.
  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE), kArbitraryTimeout);

  // EXPECT_NONFATAL_FAILURE() can only reference globals and statics.
  static RunLoop& static_loop = run_loop;
  EXPECT_NONFATAL_FAILURE(static_loop.Run(), "Run() timed out.");
}

TEST(ScopedRunLoopTimeoutTest, RunTasksUntilTimeout) {
  TaskEnvironment task_environment;
  RunLoop run_loop;

  static constexpr auto kArbitraryTimeout = Milliseconds(10);
  ScopedRunLoopTimeout run_timeout(FROM_HERE, kArbitraryTimeout);

  // Posting a task with the same delay as our timeout, immediately before
  // calling Run(), means it should get to run. Since this uses QuitWhenIdle(),
  // the Run() timeout callback should also get to run.
  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE), kArbitraryTimeout);

  // EXPECT_NONFATAL_FAILURE() can only reference globals and statics.
  static RunLoop& static_loop = run_loop;
  EXPECT_NONFATAL_FAILURE(static_loop.Run(), "Run() timed out.");
}

TEST(ScopedRunLoopTimeoutTest, TimesOutWithInheritedTimeoutValue) {
  testing::StrictMock<base::MockCallback<RepeatingCallback<std::string()>>>
      log_callback;
  TaskEnvironment task_environment;
  RunLoop run_loop;

  static constexpr auto kArbitraryTimeout = Milliseconds(10);
  ScopedRunLoopTimeout run_timeout(FROM_HERE, kArbitraryTimeout);
  ScopedRunLoopTimeout run_timeout2(FROM_HERE, std::nullopt,
                                    log_callback.Get());

  // Since the delayed task will be posted only after the message pump starts
  // running, the ScopedRunLoopTimeout will already have started to elapse,
  // so if Run() exits at the correct time then our delayed task will not run.
  SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(IgnoreResult(&SequencedTaskRunner::PostDelayedTask),
               SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
               MakeExpectedNotRunClosure(FROM_HERE), kArbitraryTimeout));

  // This task should get to run before Run() times-out.
  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE), kArbitraryTimeout);

  EXPECT_CALL(log_callback, Run).WillOnce(testing::Return(std::string()));

  // EXPECT_NONFATAL_FAILURE() can only reference globals and statics.
  static RunLoop& static_loop = run_loop;
  EXPECT_NONFATAL_FAILURE(static_loop.Run(), "Run() timed out.");
}

TEST(ScopedRunLoopTimeoutTest, RunTasksUntilTimeoutWithInheritedTimeoutValue) {
  testing::StrictMock<base::MockCallback<RepeatingCallback<std::string()>>>
      log_callback;
  TaskEnvironment task_environment;
  RunLoop run_loop;

  static constexpr auto kArbitraryTimeout = Milliseconds(10);
  ScopedRunLoopTimeout run_timeout(FROM_HERE, kArbitraryTimeout);
  ScopedRunLoopTimeout run_timeout2(FROM_HERE, std::nullopt,
                                    log_callback.Get());

  // Posting a task with the same delay as our timeout, immediately before
  // calling Run(), means it should get to run. Since this uses QuitWhenIdle(),
  // the Run() timeout callback should also get to run.
  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE), kArbitraryTimeout);

  EXPECT_CALL(log_callback, Run).WillOnce(testing::Return(std::string()));

  // EXPECT_NONFATAL_FAILURE() can only reference globals and statics.
  static RunLoop& static_loop = run_loop;
  EXPECT_NONFATAL_FAILURE(static_loop.Run(), "Run() timed out.");
}

namespace {

constexpr char kErrorMessage[] = "I like kittens!";

// Previously these tests hard-coded the file and line numbers; this function
// instead generates the expected message given any `FROM_HERE` type location.
std::string GetExpectedTimeoutMessage(const Location& from,
                                      const char* expected_message) {
  std::ostringstream oss;
  oss << "RunLoop::Run() timed out. Timeout set at " << from.function_name()
      << "@" << from.file_name() << ":" << from.line_number() << ".\n"
      << expected_message;
  return oss.str();
}

}  // namespace

TEST(ScopedRunLoopTimeoutTest, OnTimeoutLog) {
  TaskEnvironment task_environment;
  RunLoop run_loop;

  static constexpr auto kArbitraryTimeout = Milliseconds(10);
  const auto location = FROM_HERE;
  ScopedRunLoopTimeout run_timeout(
      location, kArbitraryTimeout,
      BindRepeating([]() -> std::string { return kErrorMessage; }));

  // EXPECT_NONFATAL_FAILURE() can only reference globals and statics.
  static RunLoop& static_loop = run_loop;
  EXPECT_NONFATAL_FAILURE(static_loop.Run(),
                          GetExpectedTimeoutMessage(location, kErrorMessage));
}

TEST(ScopedRunLoopTimeoutTest, OnTimeoutLogWithNestedTimeouts) {
  TaskEnvironment task_environment;
  RunLoop run_loop;

  static constexpr auto kArbitraryTimeout = Milliseconds(10);
  ScopedRunLoopTimeout run_timeout(
      FROM_HERE, base::Hours(1),
      BindRepeating([]() -> std::string { return "I like puppies!"; }));
  const auto location = FROM_HERE;
  ScopedRunLoopTimeout run_timeout2(
      location, kArbitraryTimeout,
      BindRepeating([]() -> std::string { return kErrorMessage; }));

  // EXPECT_NONFATAL_FAILURE() can only reference globals and statics.
  static RunLoop& static_loop = run_loop;
  EXPECT_NONFATAL_FAILURE(static_loop.Run(),
                          GetExpectedTimeoutMessage(location, kErrorMessage));
}

TEST(ScopedRunLoopTimeoutTest, OverwriteTimeoutCallbackForTesting) {
  TaskEnvironment task_environment;
  RunLoop run_loop;

  bool custom_handler_called = false;
  ScopedRunLoopTimeout::TimeoutCallback cb = DoNothing();
  ScopedRunLoopTimeout::SetTimeoutCallbackForTesting(
      std::make_unique<ScopedRunLoopTimeout::TimeoutCallback>(
          std::move(cb).Then(BindLambdaForTesting(
              [&custom_handler_called] { custom_handler_called = true; }))));
  static constexpr auto kArbitraryTimeout = Milliseconds(1);
  const auto location = FROM_HERE;
  ScopedRunLoopTimeout run_timeout(
      location, kArbitraryTimeout,
      BindRepeating([]() -> std::string { return kErrorMessage; }));

  // EXPECT_NONFATAL_FAILURE() can only reference globals and statics.
  static RunLoop& static_loop = run_loop;
  EXPECT_NONFATAL_FAILURE(static_loop.Run(),
                          GetExpectedTimeoutMessage(location, kErrorMessage));

  EXPECT_TRUE(custom_handler_called);

  ScopedRunLoopTimeout::SetTimeoutCallbackForTesting(nullptr);
}

}  // namespace base::test
