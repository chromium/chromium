// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_run_loop_timeout.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {

TEST(ScopedRunLoopTimeoutTest, TimesOut) {
  TaskEnvironment task_environment;
  RunLoop run_loop;

  static constexpr auto kArbitraryTimeout = TimeDelta::FromMilliseconds(10);
  ScopedRunLoopTimeout run_timeout(FROM_HERE, kArbitraryTimeout);

  // Since the delayed task will be posted only after the message pump starts
  // running, the ScopedRunLoopTimeout will already have started to elapse,
  // so if Run() exits at the correct time then our delayed task will not run.
  SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce(IgnoreResult(&SequencedTaskRunner::PostDelayedTask),
               SequencedTaskRunnerHandle::Get(), FROM_HERE,
               MakeExpectedNotRunClosure(FROM_HERE), kArbitraryTimeout));

  // This task should get to run before Run() times-out.
  SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE), kArbitraryTimeout);

  // EXPECT_FATAL_FAILURE() can only reference globals and statics.
  static RunLoop& static_loop = run_loop;
  EXPECT_FATAL_FAILURE(static_loop.Run(), "Run() timed out.");
}

TEST(ScopedRunLoopTimeoutTest, RunTasksUntilTimeout) {
  TaskEnvironment task_environment;
  RunLoop run_loop;

  static constexpr auto kArbitraryTimeout = TimeDelta::FromMilliseconds(10);
  ScopedRunLoopTimeout run_timeout(FROM_HERE, kArbitraryTimeout);

  // Posting a task with the same delay as our timeout, immediately before
  // calling Run(), means it should get to run. Since this uses QuitWhenIdle(),
  // the Run() timeout callback should also get to run.
  SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE), kArbitraryTimeout);

  // EXPECT_FATAL_FAILURE() can only reference globals and statics.
  static RunLoop& static_loop = run_loop;
  EXPECT_FATAL_FAILURE(static_loop.Run(), "Run() timed out.");
}

TEST(ScopedRunLoopTimeoutTest, OnTimeoutLog) {
  TaskEnvironment task_environment;
  RunLoop run_loop;

  static constexpr auto kArbitraryTimeout = TimeDelta::FromMilliseconds(10);
  ScopedRunLoopTimeout run_timeout(
      FROM_HERE, kArbitraryTimeout,
      BindRepeating([]() -> std::string { return "I like kittens!"; }));

  // EXPECT_FATAL_FAILURE() can only reference globals and statics.
  static RunLoop& static_loop = run_loop;
  EXPECT_FATAL_FAILURE(static_loop.Run(), "Run() timed out.\nI like kittens!");
}

}  // namespace test
}  // namespace base
