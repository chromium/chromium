// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest, Basic) {
  scoped_refptr<SingleThreadTaskRunner> task_runner(
      MakeRefCounted<TestSimpleTaskRunner>());

  EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
  {
    SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner);
    EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
    EXPECT_EQ(task_runner, SingleThreadTaskRunner::GetCurrentDefault());
  }
  EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
}

TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest, DeathOnImplicitOverride) {
  scoped_refptr<SingleThreadTaskRunner> task_runner(
      MakeRefCounted<TestSimpleTaskRunner>());
  scoped_refptr<SingleThreadTaskRunner> overidding_task_runner(
      MakeRefCounted<TestSimpleTaskRunner>());

  SingleThreadTaskRunner::CurrentDefaultHandle sttcd(task_runner);
  EXPECT_DCHECK_DEATH({
    SingleThreadTaskRunner::CurrentDefaultHandle overriding_sttcd(
        overidding_task_runner);
  });
}

TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest, OverrideExistingSTTCD) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  scoped_refptr<SingleThreadTaskRunner> task_runner_2(
      MakeRefCounted<TestSimpleTaskRunner>());
  scoped_refptr<SingleThreadTaskRunner> task_runner_3(
      MakeRefCounted<TestSimpleTaskRunner>());
  scoped_refptr<SingleThreadTaskRunner> task_runner_4(
      MakeRefCounted<TestSimpleTaskRunner>());

  EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
  {
    // STTCD in place prior to override.
    SingleThreadTaskRunner::CurrentDefaultHandle sttcd1(task_runner_1);
    EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
    EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

    {
      // Override.
      SingleThreadTaskRunner::CurrentHandleOverrideForTesting sttcd_override_2(
          task_runner_2);
      EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
      EXPECT_EQ(task_runner_2, SingleThreadTaskRunner::GetCurrentDefault());

      {
        // Nested override.
        SingleThreadTaskRunner::CurrentHandleOverrideForTesting
            sttcd_override_3(task_runner_3);
        EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
        EXPECT_EQ(task_runner_3, SingleThreadTaskRunner::GetCurrentDefault());
      }

      // Back to single override.
      EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
      EXPECT_EQ(task_runner_2, SingleThreadTaskRunner::GetCurrentDefault());

      {
        // Backup to double override with another STTCD.
        SingleThreadTaskRunner::CurrentHandleOverrideForTesting
            sttcd_override_4(task_runner_4);
        EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
        EXPECT_EQ(task_runner_4, SingleThreadTaskRunner::GetCurrentDefault());
      }
    }

    // Back to simple STTCD.
    EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
    EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());
  }
  EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
}

TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest, OverrideNoExistingSTTCD) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1(
      MakeRefCounted<TestSimpleTaskRunner>());
  scoped_refptr<SingleThreadTaskRunner> task_runner_2(
      MakeRefCounted<TestSimpleTaskRunner>());

  EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
  {
    // Override with no STTCD in place.
    SingleThreadTaskRunner::CurrentHandleOverrideForTesting sttcd_override_1(
        task_runner_1);
    EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
    EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());

    {
      // Nested override works the same.
      SingleThreadTaskRunner::CurrentHandleOverrideForTesting sttcd_override_2(
          task_runner_2);
      EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
      EXPECT_EQ(task_runner_2, SingleThreadTaskRunner::GetCurrentDefault());
    }

    // Back to single override.
    EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
    EXPECT_EQ(task_runner_1, SingleThreadTaskRunner::GetCurrentDefault());
  }
  EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
}

TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest, DeathOnSTTCDOverOverride) {
  scoped_refptr<SingleThreadTaskRunner> task_runner(
      MakeRefCounted<TestSimpleTaskRunner>());
  scoped_refptr<SingleThreadTaskRunner> overidding_task_runner(
      MakeRefCounted<TestSimpleTaskRunner>());

  SingleThreadTaskRunner::CurrentHandleOverrideForTesting sttcd_override(
      task_runner);
  EXPECT_DCHECK_DEATH({
    SingleThreadTaskRunner::CurrentDefaultHandle overriding_sttrcd(
        overidding_task_runner);
  });
}

TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest, NestedRunLoop) {
  test::SingleThreadTaskEnvironment task_environment;
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
  scoped_refptr<SingleThreadTaskRunner> task_runner(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentHandleOverride sttrcd_override(
      task_runner,
      /*allow_nested_runloop=*/true);
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
  EXPECT_EQ(task_runner, SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_EQ(task_runner, SequencedTaskRunner::GetCurrentDefault());
  RunLoop().RunUntilIdle();
}

TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest, DeathOnNestedRunLoop) {
  test::SingleThreadTaskEnvironment task_environment;
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
  scoped_refptr<SingleThreadTaskRunner> task_runner(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentHandleOverrideForTesting sttcd_override(
      task_runner);
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
  EXPECT_EQ(task_runner, SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_EQ(task_runner, SequencedTaskRunner::GetCurrentDefault());
  EXPECT_DCHECK_DEATH({ RunLoop().RunUntilIdle(); });
}

}  // namespace base
