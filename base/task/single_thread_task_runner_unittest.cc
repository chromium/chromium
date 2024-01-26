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

// Verify that instantiating a `CurrentDefaultHandle` without `MayAlreadyExist`
// fails if there is already a current default `SingleThreadTaskRunner`.
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

// Verify nested instantiations of `CurrentHandleOverrideForTesting` in a scope
// with a `CurrentDefaultHandle`.
TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest, NestedOverrideForTesting) {
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

// Same as above, but using `CurrentDefaultHandle` with `MayAlreadyExist`
// instead of `CurrentHandleOverrideForTesting`.
TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest,
     NestedOverrideWithMayAlreadyExist) {
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
      SingleThreadTaskRunner::CurrentDefaultHandle sttcd_override_2(
          task_runner_2,
          SingleThreadTaskRunner::CurrentDefaultHandle::MayAlreadyExist{});
      EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
      EXPECT_EQ(task_runner_2, SingleThreadTaskRunner::GetCurrentDefault());

      {
        // Nested override.
        SingleThreadTaskRunner::CurrentDefaultHandle sttcd_override_3(
            task_runner_3,
            SingleThreadTaskRunner::CurrentDefaultHandle::MayAlreadyExist{});
        EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
        EXPECT_EQ(task_runner_3, SingleThreadTaskRunner::GetCurrentDefault());
      }

      // Back to single override.
      EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
      EXPECT_EQ(task_runner_2, SingleThreadTaskRunner::GetCurrentDefault());

      {
        // Backup to double override with another STTCD.
        SingleThreadTaskRunner::CurrentDefaultHandle sttcd_override_4(
            task_runner_4,
            SingleThreadTaskRunner::CurrentDefaultHandle::MayAlreadyExist{});
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

// Verify that `CurrentDefaultHandle` can be used to set the current default
// `SingleThreadTaskRunner` and `SequencedTaskRunner` to null in a scope that
// already has a default.
TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest, OverrideWithNull) {
  auto tr1 = MakeRefCounted<TestSimpleTaskRunner>();

  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      tr1, SingleThreadTaskRunner::CurrentDefaultHandle::MayAlreadyExist{});
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
  EXPECT_EQ(tr1, SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
  EXPECT_EQ(tr1, SequencedTaskRunner::GetCurrentDefault());

  {
    SingleThreadTaskRunner::CurrentDefaultHandle nested_handle(
        nullptr,
        SingleThreadTaskRunner::CurrentDefaultHandle::MayAlreadyExist{});
    EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
    EXPECT_CHECK_DEATH(
        { auto tr2 = SingleThreadTaskRunner::GetCurrentDefault(); });
    EXPECT_FALSE(SequencedTaskRunner::HasCurrentDefault());
    EXPECT_CHECK_DEATH(
        { auto tr2 = SequencedTaskRunner::GetCurrentDefault(); });
  }

  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
  EXPECT_EQ(tr1, SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
  EXPECT_EQ(tr1, SequencedTaskRunner::GetCurrentDefault());
}

// Verify that `CurrentDefaultHandle` can be used to set the current default
// `SingleThreadTaskRunner` and `SequencedTaskRunner` to a non-null value in a
// scope that already has a default.
TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest, OverrideWithNonNull) {
  auto tr1 = MakeRefCounted<TestSimpleTaskRunner>();
  auto tr2 = MakeRefCounted<TestSimpleTaskRunner>();

  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      tr1, SingleThreadTaskRunner::CurrentDefaultHandle::MayAlreadyExist{});
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
  EXPECT_EQ(tr1, SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
  EXPECT_EQ(tr1, SequencedTaskRunner::GetCurrentDefault());

  {
    SingleThreadTaskRunner::CurrentDefaultHandle nested_handle(
        tr2, SingleThreadTaskRunner::CurrentDefaultHandle::MayAlreadyExist{});
    EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
    EXPECT_EQ(tr2, SingleThreadTaskRunner::GetCurrentDefault());
    EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
    EXPECT_EQ(tr2, SequencedTaskRunner::GetCurrentDefault());
  }

  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
  EXPECT_EQ(tr1, SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
  EXPECT_EQ(tr1, SequencedTaskRunner::GetCurrentDefault());
}

// Verify nested instantiations of `CurrentHandleOverrideForTesting` in a scope
// without a `CurrentDefaultHandle`.
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

// Verify that `CurrentDefaultHandle` can't be instantiated without
// `MayAlreadyExist` in the scope of a `CurrentHandleOverrideForTesting`.
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

// Verify that running a `RunLoop` is supported in the scope of a
// `CurrentDefaultHandle` with `MayAlreadyExist`.
TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest,
     NestedRunLoopAllowedUnderHandleOverride) {
  test::SingleThreadTaskEnvironment task_environment;
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
  scoped_refptr<SingleThreadTaskRunner> task_runner(
      MakeRefCounted<TestSimpleTaskRunner>());
  SingleThreadTaskRunner::CurrentDefaultHandle sttrcd_override(
      task_runner,
      SingleThreadTaskRunner::CurrentDefaultHandle::MayAlreadyExist{});
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
  EXPECT_EQ(task_runner, SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_EQ(task_runner, SequencedTaskRunner::GetCurrentDefault());
  RunLoop().RunUntilIdle();
}

// Verify that running a `RunLoop` fails in the scope of a
// `CurrentHandleOverrideForTesting`.
TEST(SingleThreadTaskRunnerCurrentDefaultHandleTest,
     NestedRunLoopDisallowedUnderHandleOverrideForTesting) {
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
