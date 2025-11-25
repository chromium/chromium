// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"

#include "base/barrier_closure.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_checker.h"
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
  EXPECT_CHECK_DEATH({
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
  EXPECT_CHECK_DEATH({
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

TEST(SingleThreadTaskRunnerMainThreadDefaultHandleTest, Basic) {
  EXPECT_CHECK_DEATH(std::ignore =
                         SingleThreadTaskRunner::GetMainThreadDefault());

  {
    scoped_refptr<SingleThreadTaskRunner> task_runner(
        MakeRefCounted<TestSimpleTaskRunner>());
    SingleThreadTaskRunner::MainThreadDefaultHandle main_thread_default_handle(
        task_runner);

    EXPECT_TRUE(SingleThreadTaskRunner::GetMainThreadDefault());
  }

  EXPECT_CHECK_DEATH(std::ignore =
                         SingleThreadTaskRunner::GetMainThreadDefault());
}

TEST(SingleThreadTaskRunnerCurrentBestEffortTest, SequenceManagerSingleQueue) {
  // TaskEnvironment wraps a SequenceManager with a single default task queue.
  test::TaskEnvironment task_env;
  EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentBestEffort());
  EXPECT_FALSE(SingleThreadTaskRunner::HasMainThreadBestEffort());
  ASSERT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
  ASSERT_TRUE(SingleThreadTaskRunner::HasMainThreadDefault());

  // Current thread is the main thread, so should return the same task runner.
  EXPECT_EQ(SingleThreadTaskRunner::GetMainThreadBestEffort(),
            SingleThreadTaskRunner::GetCurrentBestEffort());

  // Should fall back to returning the default task runner when no best-effort
  // task runner is set.
  EXPECT_EQ(SingleThreadTaskRunner::GetCurrentBestEffort(),
            SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_EQ(SingleThreadTaskRunner::GetMainThreadBestEffort(),
            SingleThreadTaskRunner::GetMainThreadDefault());
}

TEST(SingleThreadTaskRunnerCurrentBestEffortTest, SequenceManagerManyQueues) {
  // TaskEnvironmentWithMainThreadPriorities wraps a SequenceManager with
  // several task queues.
  test::TaskEnvironmentWithMainThreadPriorities task_env;
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentBestEffort());
  EXPECT_TRUE(SingleThreadTaskRunner::HasMainThreadBestEffort());
  ASSERT_TRUE(SingleThreadTaskRunner::GetCurrentBestEffort());
  ASSERT_TRUE(SingleThreadTaskRunner::GetMainThreadBestEffort());

  // Current thread is the main thread, so should return the same task runner.
  EXPECT_EQ(SingleThreadTaskRunner::GetMainThreadBestEffort(),
            SingleThreadTaskRunner::GetCurrentBestEffort());

  // The best-effort task runner should NOT be the default.
  EXPECT_NE(SingleThreadTaskRunner::GetCurrentBestEffort(),
            SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_NE(SingleThreadTaskRunner::GetMainThreadBestEffort(),
            SingleThreadTaskRunner::GetMainThreadDefault());

  // All should run tasks on the same thread. They differ only in priority.

  // Use ThreadCheckerImpl to make sure it's not a no-op in Release builds.
  ThreadCheckerImpl thread_checker;

  RunLoop run_loop;
  auto quit_closure = BarrierClosure(4, run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentBestEffort()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
                   EXPECT_TRUE(thread_checker.CalledOnValidThread())
                       << "GetCurrentBestEffort";
                 }).Then(quit_closure));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
                   EXPECT_TRUE(thread_checker.CalledOnValidThread())
                       << "GetCurrentDefault";
                 }).Then(quit_closure));
  SingleThreadTaskRunner::GetMainThreadBestEffort()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
                   EXPECT_TRUE(thread_checker.CalledOnValidThread())
                       << "GetMainThreadBestEffort";
                 }).Then(quit_closure));
  SingleThreadTaskRunner::GetMainThreadDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
                   EXPECT_TRUE(thread_checker.CalledOnValidThread())
                       << "GetMainThreadDefault";
                 }).Then(quit_closure));
  run_loop.Run();
}

TEST(SingleThreadTaskRunnerCurrentBestEffortTest, ThreadPoolSingleThreadTask) {
  test::TaskEnvironmentWithMainThreadPriorities task_env;

  // Current thread is the main thread, so should return the same task runner.
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentBestEffort());
  auto task_env_best_effort_task_runner =
      SingleThreadTaskRunner::GetCurrentBestEffort();
  EXPECT_EQ(SingleThreadTaskRunner::GetMainThreadBestEffort(),
            task_env_best_effort_task_runner);

  // Use ThreadCheckerImpl to make sure it's not a no-op in Release builds.
  ThreadCheckerImpl thread_checker;

  // Test GetCurrentBestEffort() from a default-priority task and a BEST_EFFORT
  // task.
  constexpr auto kPriorities =
      std::to_array({TaskPriority::USER_BLOCKING, TaskPriority::BEST_EFFORT});

  for (auto priority : kPriorities) {
    ThreadPool::CreateSingleThreadTaskRunner({priority})
        ->PostTask(
            FROM_HERE,
            BindLambdaForTesting([&] {
              SCOPED_TRACE(priority);

              // TODO(crbug.com/441949788): Even if this is on a BEST_EFFORT
              // task runner, HasCurrentBestEffort() returns false because it
              // only supports SequenceManager task queues.
              EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentBestEffort());
              ASSERT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());

              // Should fall back to returning the default task runner when no
              // best-effort task runner is set.
              EXPECT_EQ(SingleThreadTaskRunner::GetCurrentBestEffort(),
                        SingleThreadTaskRunner::GetCurrentDefault());

              // It should NOT be the TaskEnvironment's best-effort task runner.
              EXPECT_NE(SingleThreadTaskRunner::GetCurrentBestEffort(),
                        task_env_best_effort_task_runner);
              EXPECT_FALSE(thread_checker.CalledOnValidThread());

              // GetMainThreadBestEffort() should return the same result
              // everywhere.
              EXPECT_TRUE(SingleThreadTaskRunner::HasMainThreadBestEffort());
              EXPECT_EQ(SingleThreadTaskRunner::GetMainThreadBestEffort(),
                        task_env_best_effort_task_runner);
            }).Then(task_env.QuitClosure()));
    task_env.RunUntilQuit();
  }
}

TEST(SingleThreadTaskRunnerCurrentBestEffortTest, ThreadPoolSequencedTask) {
  test::TaskEnvironmentWithMainThreadPriorities task_env;

  // Current thread is the main thread, so should return the same task runner.
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentBestEffort());
  auto task_env_best_effort_task_runner =
      SingleThreadTaskRunner::GetCurrentBestEffort();
  EXPECT_EQ(SingleThreadTaskRunner::GetMainThreadBestEffort(),
            task_env_best_effort_task_runner);

  // Test GetCurrentBestEffort() from a default-priority task and a BEST_EFFORT
  // task.
  constexpr auto kPriorities =
      std::to_array({TaskPriority::USER_BLOCKING, TaskPriority::BEST_EFFORT});

  for (auto priority : kPriorities) {
    ThreadPool::CreateSequencedTaskRunner({priority})
        ->PostTask(
            FROM_HERE,
            BindLambdaForTesting([&] {
              SCOPED_TRACE(priority);

              // The current task isn't bound to a single thread.
              EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentBestEffort());
              EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());

              // GetMainThreadBestEffort() should return the same result
              // everywhere.
              EXPECT_TRUE(SingleThreadTaskRunner::HasMainThreadBestEffort());
              EXPECT_EQ(SingleThreadTaskRunner::GetMainThreadBestEffort(),
                        task_env_best_effort_task_runner);
            }).Then(task_env.QuitClosure()));
    task_env.RunUntilQuit();
  }
}

TEST(SingleThreadTaskRunnerCurrentBestEffortTest, ThreadPoolUnsequencedTask) {
  test::TaskEnvironmentWithMainThreadPriorities task_env;

  // Current thread is the main thread, so should return the same task runner.
  EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentBestEffort());
  auto task_env_best_effort_task_runner =
      SingleThreadTaskRunner::GetCurrentBestEffort();
  EXPECT_EQ(SingleThreadTaskRunner::GetMainThreadBestEffort(),
            task_env_best_effort_task_runner);

  // Test GetCurrentBestEffort() from a default-priority task and a BEST_EFFORT
  // task.
  constexpr auto kPriorities =
      std::to_array({TaskPriority::USER_BLOCKING, TaskPriority::BEST_EFFORT});

  for (auto priority : kPriorities) {
    ThreadPool::PostTask(
        FROM_HERE, {priority},
        BindLambdaForTesting([&] {
          SCOPED_TRACE(priority);

          // The current task isn't bound to a single thread.
          EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentBestEffort());
          EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());

          // GetMainThreadBestEffort() should return the same result everywhere.
          EXPECT_TRUE(SingleThreadTaskRunner::HasMainThreadBestEffort());
          EXPECT_EQ(SingleThreadTaskRunner::GetMainThreadBestEffort(),
                    task_env_best_effort_task_runner);
        }).Then(task_env.QuitClosure()));
    task_env.RunUntilQuit();
  }
}

TEST(SingleTaskRunnerCurrentBestEffortDeathTest, NoContext) {
  EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentBestEffort());
  EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
  // Ensure that GetCurrentBestEffort() doesn't return a value when
  // HasCurrentDefault() is false.
  EXPECT_CHECK_DEATH(
      { auto task_runner = SingleThreadTaskRunner::GetCurrentBestEffort(); });
}

TEST(SingleTaskRunnerMainThreadBestEffortDeathTest, NoContext) {
  EXPECT_FALSE(SingleThreadTaskRunner::HasMainThreadBestEffort());
  EXPECT_FALSE(SingleThreadTaskRunner::HasMainThreadDefault());
  // Ensure that GetMainThreadBestEffort() doesn't return a value when
  // HasMainThreadDefault() is false.
  EXPECT_CHECK_DEATH({
    auto task_runner = SingleThreadTaskRunner::GetMainThreadBestEffort();
  });
}

}  // namespace base
