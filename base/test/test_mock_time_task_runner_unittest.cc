// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_mock_time_task_runner.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Basic usage should work the same from default and bound
// TestMockTimeTaskRunners.
TEST(TestMockTimeTaskRunnerTest, Basic) {
  static constexpr TestMockTimeTaskRunner::Type kTestCases[] = {
      TestMockTimeTaskRunner::Type::kStandalone,
      TestMockTimeTaskRunner::Type::kBoundToThread};

  for (auto type : kTestCases) {
    SCOPED_TRACE(static_cast<int>(type));

    auto mock_time_task_runner = MakeRefCounted<TestMockTimeTaskRunner>(type);
    int counter = 0;

    mock_time_task_runner->PostTask(
        FROM_HERE, base::BindOnce([](int* counter) { *counter += 1; },
                                  Unretained(&counter)));
    mock_time_task_runner->PostTask(
        FROM_HERE, base::BindOnce([](int* counter) { *counter += 32; },
                                  Unretained(&counter)));
    mock_time_task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce([](int* counter) { *counter += 256; },
                       Unretained(&counter)),
        TimeDelta::FromSeconds(3));
    mock_time_task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce([](int* counter) { *counter += 64; },
                       Unretained(&counter)),
        TimeDelta::FromSeconds(1));
    mock_time_task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce([](int* counter) { *counter += 1024; },
                       Unretained(&counter)),
        TimeDelta::FromMinutes(20));
    mock_time_task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce([](int* counter) { *counter += 4096; },
                       Unretained(&counter)),
        TimeDelta::FromDays(20));

    int expected_value = 0;
    EXPECT_EQ(expected_value, counter);
    mock_time_task_runner->RunUntilIdle();
    expected_value += 1;
    expected_value += 32;
    EXPECT_EQ(expected_value, counter);

    mock_time_task_runner->RunUntilIdle();
    EXPECT_EQ(expected_value, counter);

    mock_time_task_runner->FastForwardBy(TimeDelta::FromSeconds(1));
    expected_value += 64;
    EXPECT_EQ(expected_value, counter);

    mock_time_task_runner->FastForwardBy(TimeDelta::FromSeconds(5));
    expected_value += 256;
    EXPECT_EQ(expected_value, counter);

    mock_time_task_runner->FastForwardUntilNoTasksRemain();
    expected_value += 1024;
    expected_value += 4096;
    EXPECT_EQ(expected_value, counter);
  }
}

// A default TestMockTimeTaskRunner shouldn't result in a thread association.
TEST(TestMockTimeTaskRunnerTest, DefaultUnbound) {
  auto unbound_mock_time_task_runner = MakeRefCounted<TestMockTimeTaskRunner>();
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());
  EXPECT_DEATH_IF_SUPPORTED({ RunLoop().RunUntilIdle(); }, "");
}

TEST(TestMockTimeTaskRunnerTest, RunLoopDriveableWhenBound) {
  auto bound_mock_time_task_runner = MakeRefCounted<TestMockTimeTaskRunner>(
      TestMockTimeTaskRunner::Type::kBoundToThread);

  int counter = 0;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce([](int* counter) { *counter += 1; },
                                Unretained(&counter)));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce([](int* counter) { *counter += 32; },
                                Unretained(&counter)));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 256; },
                     Unretained(&counter)),
      TimeDelta::FromSeconds(3));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 64; },
                     Unretained(&counter)),
      TimeDelta::FromSeconds(1));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 1024; },
                     Unretained(&counter)),
      TimeDelta::FromMinutes(20));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 4096; },
                     Unretained(&counter)),
      TimeDelta::FromDays(20));

  int expected_value = 0;
  EXPECT_EQ(expected_value, counter);
  RunLoop().RunUntilIdle();
  expected_value += 1;
  expected_value += 32;
  EXPECT_EQ(expected_value, counter);

  RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_value, counter);

  {
    RunLoop run_loop;
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TimeDelta::FromSeconds(1));
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce([](int* counter) { *counter += 8192; },
                       Unretained(&counter)),
        TimeDelta::FromSeconds(1));

    // The QuitClosure() should be ordered between the 64 and the 8192
    // increments and should preempt the latter.
    run_loop.Run();
    expected_value += 64;
    EXPECT_EQ(expected_value, counter);

    // Running until idle should process the 8192 increment whose delay has
    // expired in the previous Run().
    RunLoop().RunUntilIdle();
    expected_value += 8192;
    EXPECT_EQ(expected_value, counter);
  }

  {
    RunLoop run_loop;
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitWhenIdleClosure(), TimeDelta::FromSeconds(5));
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce([](int* counter) { *counter += 16384; },
                       Unretained(&counter)),
        TimeDelta::FromSeconds(5));

    // The QuitWhenIdleClosure() shouldn't preempt equally delayed tasks and as
    // such the 16384 increment should be processed before quitting.
    run_loop.Run();
    expected_value += 256;
    expected_value += 16384;
    EXPECT_EQ(expected_value, counter);
  }

  // Process the remaining tasks (note: do not mimic this elsewhere,
  // TestMockTimeTaskRunner::FastForwardUntilNoTasksRemain() is a better API to
  // do this, this is just done here for the purpose of extensively testing the
  // RunLoop approach).
  RunLoop run_loop;
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitWhenIdleClosure(), TimeDelta::FromDays(50));

  run_loop.Run();
  expected_value += 1024;
  expected_value += 4096;
  EXPECT_EQ(expected_value, counter);
}

TEST(TestMockTimeTaskRunnerTest, AvoidCaptureWhenBound) {
  // Make sure that capturing the active task runner --- which sometimes happens
  // unknowingly due to ThreadsafeObserverList deep within some singleton ---
  // does not keep the entire TestMockTimeTaskRunner alive, as in bound mode
  // that's a RunLoop::Delegate, and leaking that renders any further tests that
  // need RunLoop support unrunnable.
  //
  // (This used to happen when code run from ProcessAllTasksNoLaterThan grabbed
  //  the runner.).
  scoped_refptr<SingleThreadTaskRunner> captured;
  {
    auto task_runner = MakeRefCounted<TestMockTimeTaskRunner>(
        TestMockTimeTaskRunner::Type::kBoundToThread);

    task_runner->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                            captured = ThreadTaskRunnerHandle::Get();
                          }));
    task_runner->RunUntilIdle();
  }

  {
    // This should not complain about RunLoop::Delegate already existing.
    auto task_runner2 = MakeRefCounted<TestMockTimeTaskRunner>(
        TestMockTimeTaskRunner::Type::kBoundToThread);
  }
}

// Regression test that receiving the quit-when-idle signal when already empty
// works as intended (i.e. that |TestMockTimeTaskRunner::tasks_lock_cv| is
// properly signaled).
TEST(TestMockTimeTaskRunnerTest, RunLoopQuitFromIdle) {
  auto bound_mock_time_task_runner = MakeRefCounted<TestMockTimeTaskRunner>(
      TestMockTimeTaskRunner::Type::kBoundToThread);

  Thread quitting_thread("quitting thread");
  quitting_thread.Start();

  RunLoop run_loop;
  quitting_thread.task_runner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitWhenIdleClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
}

TEST(TestMockTimeTaskRunnerTest, TakePendingTasks) {
  auto task_runner = MakeRefCounted<TestMockTimeTaskRunner>();
  task_runner->PostTask(FROM_HERE, DoNothing());
  EXPECT_TRUE(task_runner->HasPendingTask());
  EXPECT_EQ(1u, task_runner->TakePendingTasks().size());
  EXPECT_FALSE(task_runner->HasPendingTask());
}

TEST(TestMockTimeTaskRunnerTest, CancelPendingTask) {
  auto task_runner = MakeRefCounted<TestMockTimeTaskRunner>();
  CancelableOnceClosure task1(DoNothing::Once());
  task_runner->PostDelayedTask(FROM_HERE, task1.callback(),
                               TimeDelta::FromSeconds(1));
  EXPECT_TRUE(task_runner->HasPendingTask());
  EXPECT_EQ(1u, task_runner->GetPendingTaskCount());
  EXPECT_EQ(TimeDelta::FromSeconds(1), task_runner->NextPendingTaskDelay());
  task1.Cancel();
  EXPECT_FALSE(task_runner->HasPendingTask());

  CancelableOnceClosure task2(DoNothing::Once());
  task_runner->PostDelayedTask(FROM_HERE, task2.callback(),
                               TimeDelta::FromSeconds(1));
  task2.Cancel();
  EXPECT_EQ(0u, task_runner->GetPendingTaskCount());

  CancelableOnceClosure task3(DoNothing::Once());
  task_runner->PostDelayedTask(FROM_HERE, task3.callback(),
                               TimeDelta::FromSeconds(1));
  task3.Cancel();
  EXPECT_EQ(TimeDelta::Max(), task_runner->NextPendingTaskDelay());

  CancelableOnceClosure task4(DoNothing::Once());
  task_runner->PostDelayedTask(FROM_HERE, task4.callback(),
                               TimeDelta::FromSeconds(1));
  task4.Cancel();
  EXPECT_TRUE(task_runner->TakePendingTasks().empty());
}

TEST(TestMockTimeTaskRunnerTest, NoFastForwardToCancelledTask) {
  auto task_runner = MakeRefCounted<TestMockTimeTaskRunner>();
  TimeTicks start_time = task_runner->NowTicks();
  CancelableOnceClosure task(DoNothing::Once());
  task_runner->PostDelayedTask(FROM_HERE, task.callback(),
                               TimeDelta::FromSeconds(1));
  EXPECT_EQ(TimeDelta::FromSeconds(1), task_runner->NextPendingTaskDelay());
  task.Cancel();
  task_runner->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(start_time, task_runner->NowTicks());
}

TEST(TestMockTimeTaskRunnerTest, AdvanceMockTickClockDoesNotRunTasks) {
  auto task_runner = MakeRefCounted<TestMockTimeTaskRunner>();
  TimeTicks start_time = task_runner->NowTicks();
  task_runner->PostTask(FROM_HERE, BindOnce([]() { ADD_FAILURE(); }));
  task_runner->PostDelayedTask(FROM_HERE, BindOnce([]() { ADD_FAILURE(); }),
                               TimeDelta::FromSeconds(1));

  task_runner->AdvanceMockTickClock(TimeDelta::FromSeconds(3));
  EXPECT_EQ(start_time + TimeDelta::FromSeconds(3), task_runner->NowTicks());
  EXPECT_EQ(2u, task_runner->GetPendingTaskCount());
}

}  // namespace base
