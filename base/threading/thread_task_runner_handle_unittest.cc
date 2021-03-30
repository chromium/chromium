// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_task_runner_handle.h"

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ThreadTaskRunnerHandleTest, Basic) {
  scoped_refptr<SingleThreadTaskRunner> task_runner(new TestSimpleTaskRunner);

  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  {
    ThreadTaskRunnerHandle ttrh1(task_runner);
    EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
    EXPECT_EQ(task_runner, ThreadTaskRunnerHandle::Get());
  }
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
}

TEST(ThreadTaskRunnerHandleTest, DeathOnImplicitOverride) {
  scoped_refptr<SingleThreadTaskRunner> task_runner(new TestSimpleTaskRunner);
  scoped_refptr<SingleThreadTaskRunner> overidding_task_runner(
      new TestSimpleTaskRunner);

  ThreadTaskRunnerHandle ttrh(task_runner);
  EXPECT_DCHECK_DEATH(
      { ThreadTaskRunnerHandle overriding_ttrh(overidding_task_runner); });
}

TEST(ThreadTaskRunnerHandleTest, OverrideExistingTTRH) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(new TestSimpleTaskRunner);
  scoped_refptr<SingleThreadTaskRunner> task_runner_2(new TestSimpleTaskRunner);
  scoped_refptr<SingleThreadTaskRunner> task_runner_3(new TestSimpleTaskRunner);
  scoped_refptr<SingleThreadTaskRunner> task_runner_4(new TestSimpleTaskRunner);

  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  {
    // TTRH in place prior to override.
    ThreadTaskRunnerHandle ttrh1(task_runner_1);
    EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
    EXPECT_EQ(task_runner_1, ThreadTaskRunnerHandle::Get());

    {
      // Override.
      ThreadTaskRunnerHandleOverrideForTesting ttrh_override_2(task_runner_2);
      EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
      EXPECT_EQ(task_runner_2, ThreadTaskRunnerHandle::Get());

      {
        // Nested override.
        ThreadTaskRunnerHandleOverrideForTesting ttrh_override_3(task_runner_3);
        EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
        EXPECT_EQ(task_runner_3, ThreadTaskRunnerHandle::Get());
      }

      // Back to single override.
      EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
      EXPECT_EQ(task_runner_2, ThreadTaskRunnerHandle::Get());

      {
        // Backup to double override with another TTRH.
        ThreadTaskRunnerHandleOverrideForTesting ttrh_override_4(task_runner_4);
        EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
        EXPECT_EQ(task_runner_4, ThreadTaskRunnerHandle::Get());
      }
    }

    // Back to simple TTRH.
    EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
    EXPECT_EQ(task_runner_1, ThreadTaskRunnerHandle::Get());
  }
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
}

TEST(ThreadTaskRunnerHandleTest, OverrideNoExistingTTRH) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(new TestSimpleTaskRunner);
  scoped_refptr<SingleThreadTaskRunner> task_runner_2(new TestSimpleTaskRunner);

  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  {
    // Override with no TTRH in place.
    ThreadTaskRunnerHandleOverrideForTesting ttrh_override_1(task_runner_1);
    EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
    EXPECT_EQ(task_runner_1, ThreadTaskRunnerHandle::Get());

    {
      // Nested override works the same.
      ThreadTaskRunnerHandleOverrideForTesting ttrh_override_2(task_runner_2);
      EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
      EXPECT_EQ(task_runner_2, ThreadTaskRunnerHandle::Get());
    }

    // Back to single override.
    EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
    EXPECT_EQ(task_runner_1, ThreadTaskRunnerHandle::Get());
  }
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
}

TEST(ThreadTaskRunnerHandleTest, DeathOnTTRHOverOverride) {
  scoped_refptr<SingleThreadTaskRunner> task_runner(new TestSimpleTaskRunner);
  scoped_refptr<SingleThreadTaskRunner> overidding_task_runner(
      new TestSimpleTaskRunner);

  ThreadTaskRunnerHandleOverrideForTesting ttrh_override(task_runner);
  EXPECT_DCHECK_DEATH(
      { ThreadTaskRunnerHandle overriding_ttrh(overidding_task_runner); });
}

TEST(ThreadTaskRunnerHandleTest, NestedRunLoop) {
  test::SingleThreadTaskEnvironment task_environment;
  EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
  scoped_refptr<SingleThreadTaskRunner> task_runner(new TestSimpleTaskRunner);
  ThreadTaskRunnerHandleOverride ttrh_override(task_runner,
                                               /*allow_nested_runloop=*/true);
  EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_EQ(task_runner, ThreadTaskRunnerHandle::Get());
  EXPECT_EQ(task_runner, SequencedTaskRunnerHandle::Get());
  RunLoop().RunUntilIdle();
}

TEST(ThreadTaskRunnerHandleTest, DeathOnNestedRunLoop) {
  test::SingleThreadTaskEnvironment task_environment;
  EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
  scoped_refptr<SingleThreadTaskRunner> task_runner(new TestSimpleTaskRunner);
  ThreadTaskRunnerHandleOverrideForTesting ttrh_override(task_runner);
  EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_EQ(task_runner, ThreadTaskRunnerHandle::Get());
  EXPECT_EQ(task_runner, SequencedTaskRunnerHandle::Get());
  EXPECT_DCHECK_DEATH({ RunLoop().RunUntilIdle(); });
}

}  // namespace base
