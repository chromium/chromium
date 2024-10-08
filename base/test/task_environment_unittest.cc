// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"

#include <atomic>
#include <memory>
#include <string_view>

#include "base/atomicops.h"
#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/debug/debugger.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/sequence_manager/time_domain.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/win/com_init_util.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>

#include "base/files/file_descriptor_watcher_posix.h"
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace base {
namespace test {

namespace {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Return;

class TaskEnvironmentTest : public testing::Test {};

void VerifyRunUntilIdleDidNotReturnAndSetFlag(
    AtomicFlag* run_until_idle_returned,
    AtomicFlag* task_ran) {
  EXPECT_FALSE(run_until_idle_returned->IsSet());
  task_ran->Set();
}

void RunUntilIdleTest(
    TaskEnvironment::ThreadPoolExecutionMode thread_pool_execution_mode) {
  AtomicFlag run_until_idle_returned;
  TaskEnvironment task_environment(thread_pool_execution_mode);

  AtomicFlag first_main_thread_task_ran;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                          Unretained(&run_until_idle_returned),
                          Unretained(&first_main_thread_task_ran)));

  AtomicFlag first_thread_pool_task_ran;
  ThreadPool::PostTask(FROM_HERE,
                       BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                                Unretained(&run_until_idle_returned),
                                Unretained(&first_thread_pool_task_ran)));

  AtomicFlag second_thread_pool_task_ran;
  AtomicFlag second_main_thread_task_ran;
  ThreadPool::PostTaskAndReply(
      FROM_HERE,
      BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
               Unretained(&run_until_idle_returned),
               Unretained(&second_thread_pool_task_ran)),
      BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
               Unretained(&run_until_idle_returned),
               Unretained(&second_main_thread_task_ran)));

  task_environment.RunUntilIdle();
  run_until_idle_returned.Set();

  EXPECT_TRUE(first_main_thread_task_ran.IsSet());
  EXPECT_TRUE(first_thread_pool_task_ran.IsSet());
  EXPECT_TRUE(second_thread_pool_task_ran.IsSet());
  EXPECT_TRUE(second_main_thread_task_ran.IsSet());
}

}  // namespace

TEST_F(TaskEnvironmentTest, QueuedRunUntilIdle) {
  RunUntilIdleTest(TaskEnvironment::ThreadPoolExecutionMode::QUEUED);
}

TEST_F(TaskEnvironmentTest, AsyncRunUntilIdle) {
  RunUntilIdleTest(TaskEnvironment::ThreadPoolExecutionMode::ASYNC);
}

// Verify that tasks posted to an ThreadPoolExecutionMode::QUEUED
// TaskEnvironment do not run outside of RunUntilIdle().
TEST_F(TaskEnvironmentTest, QueuedTasksDoNotRunOutsideOfRunUntilIdle) {
  TaskEnvironment task_environment(
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  AtomicFlag run_until_idle_called;
  ThreadPool::PostTask(FROM_HERE,
                       BindOnce(
                           [](AtomicFlag* run_until_idle_called) {
                             EXPECT_TRUE(run_until_idle_called->IsSet());
                           },
                           Unretained(&run_until_idle_called)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  run_until_idle_called.Set();
  task_environment.RunUntilIdle();

  AtomicFlag other_run_until_idle_called;
  ThreadPool::PostTask(FROM_HERE,
                       BindOnce(
                           [](AtomicFlag* other_run_until_idle_called) {
                             EXPECT_TRUE(other_run_until_idle_called->IsSet());
                           },
                           Unretained(&other_run_until_idle_called)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  other_run_until_idle_called.Set();
  task_environment.RunUntilIdle();
}

// Verify that a task posted to an ThreadPoolExecutionMode::ASYNC
// TaskEnvironment can run without a call to RunUntilIdle().
TEST_F(TaskEnvironmentTest, AsyncTasksRunAsTheyArePosted) {
  TaskEnvironment task_environment(
      TaskEnvironment::ThreadPoolExecutionMode::ASYNC);

  WaitableEvent task_ran;
  ThreadPool::PostTask(FROM_HERE,
                       BindOnce(&WaitableEvent::Signal, Unretained(&task_ran)));
  task_ran.Wait();
}

// Verify that a task posted to an ThreadPoolExecutionMode::ASYNC
// TaskEnvironment after a call to RunUntilIdle() can run without another
// call to RunUntilIdle().
TEST_F(TaskEnvironmentTest, AsyncTasksRunAsTheyArePostedAfterRunUntilIdle) {
  TaskEnvironment task_environment(
      TaskEnvironment::ThreadPoolExecutionMode::ASYNC);

  task_environment.RunUntilIdle();

  WaitableEvent task_ran;
  ThreadPool::PostTask(FROM_HERE,
                       BindOnce(&WaitableEvent::Signal, Unretained(&task_ran)));
  task_ran.Wait();
}

void DelayedTasksTest(TaskEnvironment::TimeSource time_source) {
  // Use a QUEUED execution-mode environment, so that no tasks are actually
  // executed until RunUntilIdle()/FastForwardBy() are invoked.
  TaskEnvironment task_environment(
      time_source, TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  subtle::Atomic32 counter = 0;

  constexpr base::TimeDelta kShortTaskDelay = Days(1);
  // Should run only in MOCK_TIME environment when time is fast-forwarded.
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 4);
          },
          Unretained(&counter)),
      kShortTaskDelay);
  ThreadPool::PostDelayedTask(FROM_HERE,
                              BindOnce(
                                  [](subtle::Atomic32* counter) {
                                    subtle::NoBarrier_AtomicIncrement(counter,
                                                                      128);
                                  },
                                  Unretained(&counter)),
                              kShortTaskDelay);

  constexpr base::TimeDelta kLongTaskDelay = Days(7);
  // Same as first task, longer delays to exercise
  // FastForwardUntilNoTasksRemain().
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 8);
          },
          Unretained(&counter)),
      Days(5));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 16);
          },
          Unretained(&counter)),
      kLongTaskDelay);
  ThreadPool::PostDelayedTask(FROM_HERE,
                              BindOnce(
                                  [](subtle::Atomic32* counter) {
                                    subtle::NoBarrier_AtomicIncrement(counter,
                                                                      256);
                                  },
                                  Unretained(&counter)),
                              kLongTaskDelay * 2);
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 512);
          },
          Unretained(&counter)),
      kLongTaskDelay * 3);
  ThreadPool::PostDelayedTask(FROM_HERE,
                              BindOnce(
                                  [](subtle::Atomic32* counter) {
                                    subtle::NoBarrier_AtomicIncrement(counter,
                                                                      1024);
                                  },
                                  Unretained(&counter)),
                              kLongTaskDelay * 4);

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(
                     [](subtle::Atomic32* counter) {
                       subtle::NoBarrier_AtomicIncrement(counter, 1);
                     },
                     Unretained(&counter)));
  ThreadPool::PostTask(FROM_HERE, BindOnce(
                                      [](subtle::Atomic32* counter) {
                                        subtle::NoBarrier_AtomicIncrement(
                                            counter, 2);
                                      },
                                      Unretained(&counter)));

  // This expectation will fail flakily if the preceding PostTask() is executed
  // asynchronously, indicating a problem with the QUEUED execution mode.
  int expected_value = 0;
  EXPECT_EQ(expected_value, counter);

  // RunUntilIdle() should process non-delayed tasks only in all queues.
  task_environment.RunUntilIdle();
  expected_value += 1;
  expected_value += 2;
  EXPECT_EQ(expected_value, counter);

  if (time_source == TaskEnvironment::TimeSource::MOCK_TIME) {
    const TimeTicks start_time = task_environment.NowTicks();
    const LiveTicks live_start_time = task_environment.NowLiveTicks();

    // Delay inferior to the delay of the first posted task.
    constexpr base::TimeDelta kInferiorTaskDelay = Seconds(1);
    static_assert(kInferiorTaskDelay < kShortTaskDelay,
                  "|kInferiorTaskDelay| should be "
                  "set to a value inferior to the first posted task's delay.");
    task_environment.FastForwardBy(kInferiorTaskDelay);
    EXPECT_EQ(expected_value, counter);
    // Time advances to cap even if there was no task at cap and live ticks
    // advances by the same amount.
    EXPECT_EQ(task_environment.NowTicks() - start_time, kInferiorTaskDelay);
    EXPECT_EQ(task_environment.NowLiveTicks() - live_start_time,
              kInferiorTaskDelay);

    task_environment.FastForwardBy(kShortTaskDelay - kInferiorTaskDelay);
    expected_value += 4;
    expected_value += 128;
    EXPECT_EQ(expected_value, counter);
    EXPECT_EQ(task_environment.NowTicks() - start_time, kShortTaskDelay);
    EXPECT_EQ(task_environment.NowLiveTicks() - live_start_time,
              kShortTaskDelay);

    task_environment.FastForwardUntilNoTasksRemain();
    expected_value += 8;
    expected_value += 16;
    expected_value += 256;
    expected_value += 512;
    expected_value += 1024;
    EXPECT_EQ(expected_value, counter);
    EXPECT_EQ(task_environment.NowTicks() - start_time, kLongTaskDelay * 4);
    EXPECT_EQ(task_environment.NowLiveTicks() - live_start_time,
              kLongTaskDelay * 4);
  }
}

TEST_F(TaskEnvironmentTest, DelayedTasksUnderSystemTime) {
  DelayedTasksTest(TaskEnvironment::TimeSource::SYSTEM_TIME);
}

TEST_F(TaskEnvironmentTest, DelayedTasksUnderMockTime) {
  DelayedTasksTest(TaskEnvironment::TimeSource::MOCK_TIME);
}

// Regression test for https://crbug.com/824770.
void SupportsSequenceLocalStorageOnMainThreadTest(
    TaskEnvironment::TimeSource time_source) {
  TaskEnvironment task_environment(
      time_source, TaskEnvironment::ThreadPoolExecutionMode::ASYNC);

  SequenceLocalStorageSlot<int> sls_slot;
  sls_slot.emplace(5);
  EXPECT_EQ(5, *sls_slot);
}

TEST_F(TaskEnvironmentTest, SupportsSequenceLocalStorageOnMainThread) {
  SupportsSequenceLocalStorageOnMainThreadTest(
      TaskEnvironment::TimeSource::SYSTEM_TIME);
}

TEST_F(TaskEnvironmentTest,
       SupportsSequenceLocalStorageOnMainThreadWithMockTime) {
  SupportsSequenceLocalStorageOnMainThreadTest(
      TaskEnvironment::TimeSource::MOCK_TIME);
}

// Verify that the right MessagePump is instantiated under each MainThreadType.
// This avoids having to run all other TaskEnvironmentTests in every
// MainThreadType which is redundant (message loop and message pump tests
// otherwise cover the advanced functionality provided by UI/IO pumps).
TEST_F(TaskEnvironmentTest, MainThreadType) {
  // Uses CurrentThread as a convenience accessor but could be replaced by
  // different accessors when we get rid of CurrentThread.
  EXPECT_FALSE(CurrentThread::IsSet());
  EXPECT_FALSE(CurrentUIThread::IsSet());
  EXPECT_FALSE(CurrentIOThread::IsSet());
  {
    TaskEnvironment task_environment;
    EXPECT_TRUE(CurrentThread::IsSet());
    EXPECT_FALSE(CurrentUIThread::IsSet());
    EXPECT_FALSE(CurrentIOThread::IsSet());
  }
  {
    TaskEnvironment task_environment(TaskEnvironment::MainThreadType::UI);
    EXPECT_TRUE(CurrentThread::IsSet());
    EXPECT_TRUE(CurrentUIThread::IsSet());
    EXPECT_FALSE(CurrentIOThread::IsSet());
  }
  {
    TaskEnvironment task_environment(TaskEnvironment::MainThreadType::IO);
    EXPECT_TRUE(CurrentThread::IsSet());
    EXPECT_FALSE(CurrentUIThread::IsSet());
    EXPECT_TRUE(CurrentIOThread::IsSet());
  }
  EXPECT_FALSE(CurrentThread::IsSet());
  EXPECT_FALSE(CurrentUIThread::IsSet());
  EXPECT_FALSE(CurrentIOThread::IsSet());
}

#if BUILDFLAG(IS_POSIX)
TEST_F(TaskEnvironmentTest, SupportsFileDescriptorWatcherOnIOMainThread) {
  TaskEnvironment task_environment(TaskEnvironment::MainThreadType::IO);

  int pipe_fds_[2];
  ASSERT_EQ(0, pipe(pipe_fds_));

  RunLoop run_loop;

  // The write end of a newly created pipe is immediately writable.
  auto controller = FileDescriptorWatcher::WatchWritable(
      pipe_fds_[1], run_loop.QuitClosure());

  // This will hang if the notification doesn't occur as expected.
  run_loop.Run();
}

TEST_F(TaskEnvironmentTest,
       SupportsFileDescriptorWatcherOnIOMockTimeMainThread) {
  TaskEnvironment task_environment(TaskEnvironment::MainThreadType::IO,
                                   TaskEnvironment::TimeSource::MOCK_TIME);

  int pipe_fds_[2];
  ASSERT_EQ(0, pipe(pipe_fds_));

  RunLoop run_loop;

  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&] {
        int64_t x = 1;
        auto ret = write(pipe_fds_[1], &x, sizeof(x));
        ASSERT_EQ(static_cast<size_t>(ret), sizeof(x));
      }),
      Hours(1));

  auto controller = FileDescriptorWatcher::WatchReadable(
      pipe_fds_[0], run_loop.QuitClosure());

  // This will hang if the notification doesn't occur as expected (Run() should
  // fast-forward-time when idle).
  run_loop.Run();
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(TaskEnvironmentTest, MockTimeStartsWithWholeMilliseconds) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);
  const TickClock* mock_tick_clock = task_environment.GetMockTickClock();
  const Clock* mock_clock = task_environment.GetMockClock();
  EXPECT_TRUE(
      (mock_tick_clock->NowTicks().since_origin() % Milliseconds(1)).is_zero());
  // The Windows epoch has no submillisecond components, so any submillisecond
  // components in `Time::Now()` will appear in their difference.
  EXPECT_TRUE((mock_clock->Now().since_origin() % Milliseconds(1)).is_zero());
  EXPECT_TRUE((Time::Now().since_origin() % Milliseconds(1)).is_zero());
  EXPECT_TRUE((TimeTicks::Now().since_origin() % Milliseconds(1)).is_zero());
}

// Verify that the TickClock returned by
// |TaskEnvironment::GetMockTickClock| gets updated when the
// FastForward(By|UntilNoTasksRemain) functions are called.
TEST_F(TaskEnvironmentTest, FastForwardAdvancesTickClock) {
  // Use a QUEUED execution-mode environment, so that no tasks are actually
  // executed until RunUntilIdle()/FastForwardBy() are invoked.
  TaskEnvironment task_environment(
      TaskEnvironment::TimeSource::MOCK_TIME,
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  constexpr base::TimeDelta kShortTaskDelay = Days(1);
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::DoNothing(), kShortTaskDelay);

  constexpr base::TimeDelta kLongTaskDelay = Days(7);
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::DoNothing(), kLongTaskDelay);

  const base::TickClock* tick_clock = task_environment.GetMockTickClock();
  base::TimeTicks tick_clock_ref = tick_clock->NowTicks();

  // Make sure that |FastForwardBy| advances the clock.
  task_environment.FastForwardBy(kShortTaskDelay);
  EXPECT_EQ(kShortTaskDelay, tick_clock->NowTicks() - tick_clock_ref);

  // Make sure that |FastForwardUntilNoTasksRemain| advances the clock.
  task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(kLongTaskDelay, tick_clock->NowTicks() - tick_clock_ref);

  // Fast-forwarding to a time at which there's no tasks should also advance the
  // clock.
  task_environment.FastForwardBy(kLongTaskDelay);
  EXPECT_EQ(kLongTaskDelay * 2, tick_clock->NowTicks() - tick_clock_ref);
}

TEST_F(TaskEnvironmentTest, FastForwardAdvancesMockClock) {
  constexpr base::TimeDelta kDelay = Seconds(42);
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const Clock* clock = task_environment.GetMockClock();
  const Time start_time = clock->Now();
  task_environment.FastForwardBy(kDelay);

  EXPECT_EQ(start_time + kDelay, clock->Now());
}

TEST_F(TaskEnvironmentTest, FastForwardAdvancesTime) {
  constexpr base::TimeDelta kDelay = Seconds(42);
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const Time start_time = base::Time::Now();
  task_environment.FastForwardBy(kDelay);
  EXPECT_EQ(start_time + kDelay, base::Time::Now());
}

TEST_F(TaskEnvironmentTest, FastForwardAdvancesTimeTicks) {
  constexpr base::TimeDelta kDelay = Seconds(42);
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const TimeTicks start_time = base::TimeTicks::Now();
  task_environment.FastForwardBy(kDelay);
  EXPECT_EQ(start_time + kDelay, base::TimeTicks::Now());
}

TEST_F(TaskEnvironmentTest, AdvanceClockAdvancesTickClock) {
  constexpr base::TimeDelta kDelay = Seconds(42);
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const base::TickClock* tick_clock = task_environment.GetMockTickClock();
  const base::TimeTicks start_time = tick_clock->NowTicks();
  task_environment.AdvanceClock(kDelay);

  EXPECT_EQ(start_time + kDelay, tick_clock->NowTicks());
}

TEST_F(TaskEnvironmentTest, AdvanceClockAdvancesMockClock) {
  constexpr base::TimeDelta kDelay = Seconds(42);
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const Clock* clock = task_environment.GetMockClock();
  const Time start_time = clock->Now();
  task_environment.AdvanceClock(kDelay);

  EXPECT_EQ(start_time + kDelay, clock->Now());
}

TEST_F(TaskEnvironmentTest, AdvanceClockAdvancesTime) {
  constexpr base::TimeDelta kDelay = Seconds(42);
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const Time start_time = base::Time::Now();
  task_environment.AdvanceClock(kDelay);
  EXPECT_EQ(start_time + kDelay, base::Time::Now());
}

TEST_F(TaskEnvironmentTest, AdvanceClockAdvancesTimeTicks) {
  constexpr base::TimeDelta kDelay = Seconds(42);
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const TimeTicks start_time = base::TimeTicks::Now();
  task_environment.AdvanceClock(kDelay);
  EXPECT_EQ(start_time + kDelay, base::TimeTicks::Now());
}

TEST_F(TaskEnvironmentTest, AdvanceClockAdvancesLiveTicks) {
  constexpr base::TimeDelta kDelay = Seconds(42);
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const LiveTicks start_time = base::LiveTicks::Now();
  task_environment.AdvanceClock(kDelay);
  EXPECT_EQ(start_time + kDelay, base::LiveTicks::Now());
}

TEST_F(TaskEnvironmentTest, SuspendedAdvanceClockDoesntAdvanceLiveTicks) {
  constexpr base::TimeDelta kDelay = Seconds(42);
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const TimeTicks start_time = base::TimeTicks::Now();
  const LiveTicks live_start_time = base::LiveTicks::Now();
  task_environment.SuspendedAdvanceClock(kDelay);
  EXPECT_EQ(live_start_time, base::LiveTicks::Now());
  EXPECT_EQ(start_time + kDelay, base::TimeTicks::Now());
}

TEST_F(TaskEnvironmentTest, AdvanceClockDoesNotRunTasks) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  constexpr base::TimeDelta kTaskDelay = Days(1);
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::DoNothing(), kTaskDelay);

  EXPECT_EQ(1U, task_environment.GetPendingMainThreadTaskCount());
  EXPECT_TRUE(task_environment.NextTaskIsDelayed());

  task_environment.AdvanceClock(kTaskDelay);

  // The task is still pending, but is now runnable.
  EXPECT_EQ(1U, task_environment.GetPendingMainThreadTaskCount());
  EXPECT_FALSE(task_environment.NextTaskIsDelayed());
}

TEST_F(TaskEnvironmentTest, SuspendedAdvanceClockDoesNotRunTasks) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  constexpr base::TimeDelta kTaskDelay = Days(1);
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::DoNothing(), kTaskDelay);

  EXPECT_EQ(1U, task_environment.GetPendingMainThreadTaskCount());
  EXPECT_TRUE(task_environment.NextTaskIsDelayed());

  task_environment.SuspendedAdvanceClock(kTaskDelay);

  // The task is still pending, but is now runnable.
  EXPECT_EQ(1U, task_environment.GetPendingMainThreadTaskCount());
  EXPECT_FALSE(task_environment.NextTaskIsDelayed());
}

TEST_F(TaskEnvironmentTest, AdvanceClockSchedulesRipeDelayedTasks) {
  TaskEnvironment task_environment(
      TaskEnvironment::TimeSource::MOCK_TIME,
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  bool ran = false;

  constexpr base::TimeDelta kTaskDelay = Days(1);
  ThreadPool::PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([&] { ran = true; }), kTaskDelay);

  task_environment.AdvanceClock(kTaskDelay);
  EXPECT_FALSE(ran);
  task_environment.RunUntilIdle();
  EXPECT_TRUE(ran);
}

TEST_F(TaskEnvironmentTest, SuspendedAdvanceClockSchedulesRipeDelayedTasks) {
  TaskEnvironment task_environment(
      TaskEnvironment::TimeSource::MOCK_TIME,
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  bool ran = false;

  constexpr base::TimeDelta kTaskDelay = Days(1);
  ThreadPool::PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([&] { ran = true; }), kTaskDelay);

  task_environment.SuspendedAdvanceClock(kTaskDelay);
  EXPECT_FALSE(ran);
  task_environment.RunUntilIdle();
  EXPECT_TRUE(ran);
}

// Verify that FastForwardBy() runs existing immediate tasks before advancing,
// then advances to the next delayed task, runs it, then advances the remainder
// of time when out of tasks.
TEST_F(TaskEnvironmentTest, FastForwardOnlyAdvancesWhenIdle) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const TimeTicks start_time = base::TimeTicks::Now();

  constexpr base::TimeDelta kDelay = Seconds(42);
  constexpr base::TimeDelta kFastForwardUntil = Seconds(100);
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting(
                     [&] { EXPECT_EQ(start_time, base::TimeTicks::Now()); }));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_EQ(start_time + kDelay, base::TimeTicks::Now());
      }),
      kDelay);
  task_environment.FastForwardBy(kFastForwardUntil);
  EXPECT_EQ(start_time + kFastForwardUntil, base::TimeTicks::Now());
}

// Verify that SuspendedFastForwardBy() behaves as FastForwardBy() but doesn't
// advance `LiveTicks`
TEST_F(TaskEnvironmentTest, SuspendedFastForwardOnlyAdvancesWhenIdle) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const TimeTicks start_time = base::TimeTicks::Now();
  const LiveTicks live_start_time = base::LiveTicks::Now();

  constexpr base::TimeDelta kDelay = Seconds(42);
  constexpr base::TimeDelta kFastForwardUntil = Seconds(100);
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_EQ(start_time, base::TimeTicks::Now());
        EXPECT_EQ(live_start_time, base::LiveTicks::Now());
      }));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_EQ(start_time + kDelay, base::TimeTicks::Now());
        EXPECT_EQ(live_start_time, base::LiveTicks::Now());
      }),
      kDelay);
  task_environment.SuspendedFastForwardBy(kFastForwardUntil);
  EXPECT_EQ(start_time + kFastForwardUntil, base::TimeTicks::Now());
  EXPECT_EQ(live_start_time, base::LiveTicks::Now());
}

// FastForwardBy(0) should be equivalent of RunUntilIdle().
TEST_F(TaskEnvironmentTest, FastForwardZero) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  std::atomic_int run_count{0};

  for (int i = 0; i < 1000; ++i) {
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindLambdaForTesting([&] {
          run_count.fetch_add(1, std::memory_order_relaxed);
        }));
    ThreadPool::PostTask(FROM_HERE, BindLambdaForTesting([&] {
                           run_count.fetch_add(1, std::memory_order_relaxed);
                         }));
  }

  task_environment.FastForwardBy(base::TimeDelta());

  EXPECT_EQ(2000, run_count.load(std::memory_order_relaxed));
}

TEST_F(TaskEnvironmentTest, NestedFastForwardBy) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  constexpr TimeDelta kDelayPerTask = Milliseconds(1);
  const TimeTicks start_time = task_environment.NowTicks();
  const LiveTicks live_start_time = task_environment.NowLiveTicks();

  int max_nesting_level = 0;

  RepeatingClosure post_fast_forwarding_task;
  post_fast_forwarding_task = BindLambdaForTesting([&] {
    if (max_nesting_level < 5) {
      ++max_nesting_level;
      SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, post_fast_forwarding_task, kDelayPerTask);
      task_environment.FastForwardBy(kDelayPerTask);
    }
  });
  post_fast_forwarding_task.Run();

  EXPECT_EQ(max_nesting_level, 5);
  EXPECT_EQ(task_environment.NowTicks(), start_time + kDelayPerTask * 5);
  EXPECT_EQ(task_environment.NowLiveTicks(),
            live_start_time + kDelayPerTask * 5);
}

TEST_F(TaskEnvironmentTest, NestedRunInFastForwardBy) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  constexpr TimeDelta kDelayPerTask = Milliseconds(1);
  const TimeTicks start_time = task_environment.NowTicks();
  const LiveTicks live_start_time = task_environment.NowLiveTicks();

  std::vector<RunLoop*> run_loops;

  RepeatingClosure post_and_runloop_task;
  post_and_runloop_task = BindLambdaForTesting([&] {
    // Run 4 nested run loops on top of the initial FastForwardBy().
    if (run_loops.size() < 4U) {
      SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, post_and_runloop_task, kDelayPerTask);

      RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
      run_loops.push_back(&run_loop);
      run_loop.Run();
    } else {
      for (RunLoop* run_loop : run_loops) {
        run_loop->Quit();
      }
    }
  });

  // Initial task is FastForwardBy().
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, post_and_runloop_task, kDelayPerTask);
  task_environment.FastForwardBy(kDelayPerTask);

  EXPECT_EQ(run_loops.size(), 4U);
  EXPECT_EQ(task_environment.NowTicks(), start_time + kDelayPerTask * 5);
  EXPECT_EQ(task_environment.NowLiveTicks(),
            live_start_time + kDelayPerTask * 5);
}

TEST_F(TaskEnvironmentTest,
       CrossThreadImmediateTaskPostingDoesntAffectMockTime) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  int count = 0;

  // Post tasks delayd between 0 and 999 seconds.
  for (int i = 0; i < 1000; ++i) {
    const TimeDelta delay = Seconds(i);
    SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        BindOnce(
            [](TimeTicks expected_run_time, int* count) {
              EXPECT_EQ(expected_run_time, TimeTicks::Now());
              ++*count;
            },
            TimeTicks::Now() + delay, &count),
        delay);
  }

  // Having a bunch of tasks running in parallel and replying to the main thread
  // shouldn't affect the rest of this test. Wait for the first task to run
  // before proceeding with the test to increase the likelihood of exercising
  // races.
  base::WaitableEvent first_reply_is_incoming;
  for (int i = 0; i < 1000; ++i) {
    ThreadPool::PostTaskAndReply(
        FROM_HERE,
        BindOnce(&WaitableEvent::Signal, Unretained(&first_reply_is_incoming)),
        DoNothing());
  }
  first_reply_is_incoming.Wait();

  task_environment.FastForwardBy(Seconds(1000));

  // If this test flakes it's because there's an error with MockTimeDomain.
  EXPECT_EQ(count, 1000);

  // Flush any remaining asynchronous tasks with Unretained() state.
  task_environment.RunUntilIdle();
}

TEST_F(TaskEnvironmentTest, MultiThreadedMockTime) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  constexpr TimeDelta kOneMs = Milliseconds(1);
  const TimeTicks start_time = task_environment.NowTicks();
  const TimeTicks end_time = start_time + Milliseconds(1'000);

  // Last TimeTicks::Now() seen from either contexts.
  TimeTicks last_main_thread_ticks = start_time;
  TimeTicks last_thread_pool_ticks = start_time;

  RepeatingClosure post_main_thread_delayed_task;
  post_main_thread_delayed_task = BindLambdaForTesting([&] {
    // Expect that time only moves forward.
    EXPECT_GE(task_environment.NowTicks(), last_main_thread_ticks);

    // Post four tasks to exercise the system some more but only if this is the
    // first task at its runtime (otherwise we end up with 4^10'000 tasks by
    // the end!).
    if (last_main_thread_ticks < task_environment.NowTicks() &&
        task_environment.NowTicks() < end_time) {
      SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, post_main_thread_delayed_task, kOneMs);
      SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, post_main_thread_delayed_task, kOneMs);
      SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, post_main_thread_delayed_task, kOneMs);
      SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, post_main_thread_delayed_task, kOneMs);
    }

    last_main_thread_ticks = task_environment.NowTicks();
  });

  RepeatingClosure post_thread_pool_delayed_task;
  post_thread_pool_delayed_task = BindLambdaForTesting([&] {
    // Expect that time only moves forward.
    EXPECT_GE(task_environment.NowTicks(), last_thread_pool_ticks);

    // Post four tasks to exercise the system some more but only if this is the
    // first task at its runtime (otherwise we end up with 4^10'000 tasks by
    // the end!).
    if (last_thread_pool_ticks < task_environment.NowTicks() &&
        task_environment.NowTicks() < end_time) {
      SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, post_thread_pool_delayed_task, kOneMs);
      SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, post_thread_pool_delayed_task, kOneMs);
      SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, post_thread_pool_delayed_task, kOneMs);
      SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, post_thread_pool_delayed_task, kOneMs);

      EXPECT_LT(task_environment.NowTicks(), end_time);
    }

    last_thread_pool_ticks = task_environment.NowTicks();
  });

  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, post_main_thread_delayed_task, kOneMs);
  ThreadPool::CreateSequencedTaskRunner({})->PostDelayedTask(
      FROM_HERE, post_thread_pool_delayed_task, kOneMs);

  task_environment.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(last_main_thread_ticks, end_time);
  EXPECT_EQ(last_thread_pool_ticks, end_time);
  EXPECT_EQ(task_environment.NowTicks(), end_time);
}

// This test ensures the implementation of FastForwardBy() doesn't fast-forward
// beyond the cap it reaches idle with pending delayed tasks further ahead on
// the main thread.
TEST_F(TaskEnvironmentTest, MultiThreadedFastForwardBy) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const TimeTicks start_time = task_environment.NowTicks();
  const LiveTicks live_start_time = task_environment.NowLiveTicks();

  // The 1s delayed task in the pool should run but not the 5s delayed task on
  // the main thread and fast-forward by should be capped at +2s.
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, MakeExpectedNotRunClosure(FROM_HERE), Seconds(5));
  ThreadPool::PostDelayedTask(FROM_HERE, {}, MakeExpectedRunClosure(FROM_HERE),
                              Seconds(1));
  task_environment.FastForwardBy(Seconds(2));

  EXPECT_EQ(task_environment.NowTicks(), start_time + Seconds(2));
  EXPECT_EQ(task_environment.NowLiveTicks(), live_start_time + Seconds(2));
}

// Verify that ThreadPoolExecutionMode::QUEUED doesn't prevent running tasks and
// advancing time on the main thread.
TEST_F(TaskEnvironmentTest, MultiThreadedMockTimeAndThreadPoolQueuedMode) {
  TaskEnvironment task_environment(
      TaskEnvironment::TimeSource::MOCK_TIME,
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  // Atomic because it's updated from concurrent tasks in the ThreadPool
  // (could use std::memory_order_releaxed on all accesses but keeping implicit
  // operators because the test reads better that way).
  std::atomic_int count = 0;
  const TimeTicks start_time = task_environment.NowTicks();

  RunLoop run_loop;

  // Neither of these should run automatically per
  // ThreadPoolExecutionMode::QUEUED.
  ThreadPool::PostTask(FROM_HERE, BindLambdaForTesting([&] { count += 128; }));
  ThreadPool::PostDelayedTask(
      FROM_HERE, {}, BindLambdaForTesting([&] { count += 256; }), Seconds(5));

  // Time should auto-advance to +500s in RunLoop::Run() without having to run
  // the above forcefully QUEUED tasks.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] { count += 1; }));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&] {
        count += 2;
        run_loop.Quit();
      }),
      Seconds(500));

  int expected_value = 0;
  EXPECT_EQ(expected_value, count);
  run_loop.Run();
  expected_value += 1;
  expected_value += 2;
  EXPECT_EQ(expected_value, count);
  EXPECT_EQ(task_environment.NowTicks() - start_time, Seconds(500));

  // Fast-forward through all remaining tasks, this should unblock QUEUED tasks
  // in the thread pool but shouldn't need to advance time to process them.
  task_environment.FastForwardUntilNoTasksRemain();
  expected_value += 128;
  expected_value += 256;
  EXPECT_EQ(expected_value, count);
  EXPECT_EQ(task_environment.NowTicks() - start_time, Seconds(500));

  // Test advancing time to a QUEUED task in the future.
  ThreadPool::PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&] { count += 512; }), Seconds(5));
  task_environment.FastForwardBy(Seconds(7));
  expected_value += 512;
  EXPECT_EQ(expected_value, count);
  EXPECT_EQ(task_environment.NowTicks() - start_time, Seconds(507));

  // Confirm that QUEUED mode is still active after the above fast forwarding
  // (only the main thread task should run from RunLoop).
  ThreadPool::PostTask(FROM_HERE, BindLambdaForTesting([&] { count += 1024; }));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] { count += 2048; }));
  PlatformThread::Sleep(Milliseconds(1));
  RunLoop().RunUntilIdle();
  expected_value += 2048;
  EXPECT_EQ(expected_value, count);
  EXPECT_EQ(task_environment.NowTicks() - start_time, Seconds(507));

  // Run the remaining task to avoid use-after-free on |count| from
  // ~TaskEnvironment().
  task_environment.RunUntilIdle();
  expected_value += 1024;
  EXPECT_EQ(expected_value, count);
}

#if BUILDFLAG(IS_WIN)
// Regression test to ensure that TaskEnvironment enables the MTA in the
// thread pool (so that the test environment matches that of the browser process
// and com_init_util.h's assertions are happy in unit tests).
TEST_F(TaskEnvironmentTest, ThreadPoolPoolAllowsMTA) {
  TaskEnvironment task_environment;
  ThreadPool::PostTask(FROM_HERE, BindOnce(&win::AssertComApartmentType,
                                           win::ComApartmentType::MTA));
  task_environment.RunUntilIdle();
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(TaskEnvironmentTest, SetsDefaultRunTimeout) {
  const RunLoop::RunLoopTimeout* old_run_timeout =
      ScopedRunLoopTimeout::GetTimeoutForCurrentThread();

  {
    TaskEnvironment task_environment;

    // TaskEnvironment should set a default Run() timeout that fails the
    // calling test (before test_launcher_timeout()).

    const RunLoop::RunLoopTimeout* run_timeout =
        ScopedRunLoopTimeout::GetTimeoutForCurrentThread();
    EXPECT_NE(run_timeout, old_run_timeout);
    EXPECT_TRUE(run_timeout);
    if (!debug::BeingDebugged()) {
      EXPECT_LT(run_timeout->timeout, TestTimeouts::test_launcher_timeout());
    }
    static auto& static_on_timeout_cb = run_timeout->on_timeout;
#if defined(__clang__) && defined(_MSC_VER)
    EXPECT_NONFATAL_FAILURE(
        static_on_timeout_cb.Run(FROM_HERE),
        "RunLoop::Run() timed out. Timeout set at "
        // We don't test the line number but it would be present.
        "TaskEnvironment@base\\test\\task_environment.cc:");
#else
    EXPECT_NONFATAL_FAILURE(
        static_on_timeout_cb.Run(FROM_HERE),
        "RunLoop::Run() timed out. Timeout set at "
        // We don't test the line number but it would be present.
        "TaskEnvironment@base/test/task_environment.cc:");
#endif
  }

  EXPECT_EQ(ScopedRunLoopTimeout::GetTimeoutForCurrentThread(),
            old_run_timeout);
}

TEST_F(TaskEnvironmentTest, DescribeCurrentTasksHasPendingMainThreadTasks) {
  TaskEnvironment task_environment;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE, DoNothing());

  test::MockLog mock_log;
  mock_log.StartCapturingLogs();

  // Thread pool tasks (none here) are logged.
  EXPECT_CALL(mock_log, Log(::logging::LOGGING_INFO, _, _, _,
                            HasSubstr("ThreadPool currently running tasks")))
      .WillOnce(Return(true));
  // The pending task posted above to the main thread is logged.
  EXPECT_CALL(mock_log, Log(::logging::LOGGING_INFO, _, _, _,
                            HasSubstr("task_environment_unittest.cc")))
      .WillOnce(Return(true));
  task_environment.DescribeCurrentTasks();

  task_environment.RunUntilIdle();

  // Thread pool tasks (none here) are logged.
  EXPECT_CALL(mock_log, Log(::logging::LOGGING_INFO, _, _, _,
                            HasSubstr("ThreadPool currently running tasks")))
      .WillOnce(Return(true));
  // Pending tasks (none left) are logged.
  EXPECT_CALL(mock_log, Log(::logging::LOGGING_INFO, _, _, _,
                            HasSubstr("\"immediate_work_queue_size\":0")))
      .WillOnce(Return(true));
  task_environment.DescribeCurrentTasks();
}

TEST_F(TaskEnvironmentTest, DescribeCurrentTasksHasThreadPoolTasks) {
  TaskEnvironment task_environment;

  // Let the test block until the thread pool task is running.
  base::WaitableEvent wait_for_thread_pool_task_start;
  // Let the thread pool task block until the test has a chance to see it
  // running.
  base::WaitableEvent block_thread_pool_task;

  scoped_refptr<SequencedTaskRunner> thread_pool_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {WithBaseSyncPrimitives(), TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  thread_pool_task_runner->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                                      // The test waits until this task is
                                      // running.
                                      wait_for_thread_pool_task_start.Signal();
                                      // Wait until the test is done with this
                                      // task.
                                      block_thread_pool_task.Wait();
                                    }));
  wait_for_thread_pool_task_start.Wait();

  test::MockLog mock_log;
  mock_log.StartCapturingLogs();

  // The pending task posted above is logged.
  EXPECT_CALL(mock_log, Log(::logging::LOGGING_INFO, _, _, _,
                            HasSubstr("task_environment_unittest.cc")))
      .WillOnce(Return(true));
  // Pending tasks (none here) are logged.
  EXPECT_CALL(mock_log, Log(::logging::LOGGING_INFO, _, _, _,
                            HasSubstr("\"immediate_work_queue_size\":0")))
      .WillOnce(Return(true));
  task_environment.DescribeCurrentTasks();

  block_thread_pool_task.Signal();
  // Wait for the thread pool task to complete.
  task_environment.RunUntilIdle();

  // The current thread pool tasks (none left) are logged.
  EXPECT_CALL(mock_log, Log(::logging::LOGGING_INFO, _, _, _,
                            Not(HasSubstr("task_environment_unittest.cc"))))
      .WillOnce(Return(true));
  // Main thread pending tasks (none here) are logged.
  EXPECT_CALL(mock_log, Log(::logging::LOGGING_INFO, _, _, _,
                            HasSubstr("\"immediate_work_queue_size\":0")))
      .WillOnce(Return(true));
  task_environment.DescribeCurrentTasks();
}

TEST_F(TaskEnvironmentTest, Basic) {
  TaskEnvironment task_environment(
      TaskEnvironment::TimeSource::MOCK_TIME,
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  int counter = 0;

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 1; }, Unretained(&counter)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 32; }, Unretained(&counter)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 256; }, Unretained(&counter)),
      Seconds(3));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 64; }, Unretained(&counter)),
      Seconds(1));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 1024; }, Unretained(&counter)),
      Minutes(20));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 4096; }, Unretained(&counter)),
      Days(20));

  int expected_value = 0;
  EXPECT_EQ(expected_value, counter);
  task_environment.RunUntilIdle();
  expected_value += 1;
  expected_value += 32;
  EXPECT_EQ(expected_value, counter);

  task_environment.RunUntilIdle();
  EXPECT_EQ(expected_value, counter);

  task_environment.FastForwardBy(Seconds(1));
  expected_value += 64;
  EXPECT_EQ(expected_value, counter);

  task_environment.FastForwardBy(Seconds(5));
  expected_value += 256;
  EXPECT_EQ(expected_value, counter);

  task_environment.FastForwardUntilNoTasksRemain();
  expected_value += 1024;
  expected_value += 4096;
  EXPECT_EQ(expected_value, counter);
}

TEST_F(TaskEnvironmentTest, RunLoopDriveable) {
  TaskEnvironment task_environment(
      TaskEnvironment::TimeSource::MOCK_TIME,
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  int counter = 0;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce([](int* counter) { *counter += 1; },
                                Unretained(&counter)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce([](int* counter) { *counter += 32; },
                                Unretained(&counter)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 256; },
                     Unretained(&counter)),
      Seconds(3));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 64; },
                     Unretained(&counter)),
      Seconds(1));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 1024; },
                     Unretained(&counter)),
      Minutes(20));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 4096; },
                     Unretained(&counter)),
      Days(20));

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
    SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), Seconds(1));
    SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce([](int* counter) { *counter += 8192; },
                       Unretained(&counter)),
        Seconds(1));

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
    SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitWhenIdleClosure(), Seconds(5));
    SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce([](int* counter) { *counter += 16384; },
                       Unretained(&counter)),
        Seconds(5));

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

  // Disable Run() timeout here, otherwise we'll fast-forward to it before we
  // reach the quit task.
  ScopedDisableRunLoopTimeout disable_timeout;

  RunLoop run_loop;
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitWhenIdleClosure(), Days(50));

  run_loop.Run();
  expected_value += 1024;
  expected_value += 4096;
  EXPECT_EQ(expected_value, counter);
}

// Regression test for crbug.com/1263149
TEST_F(TaskEnvironmentTest, RunLoopGetsTurnAfterYieldingToPool) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  base::RunLoop run_loop;
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
  ThreadPool::PostTask(FROM_HERE, base::DoNothing());

  run_loop.Run();
}

// Regression test for crbug.com/1263149#c4
TEST_F(TaskEnvironmentTest, ThreadPoolAdvancesTimeUnderIdleMainThread) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  base::RunLoop run_loop;
  ThreadPool::PostDelayedTask(FROM_HERE, base::DoNothing(), base::Seconds(1));
  ThreadPool::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                              base::Seconds(2));

  run_loop.Run();
}

// Regression test for
// https://chromium-review.googlesource.com/c/chromium/src/+/3255105/5 which
// incorrectly tried to address crbug.com/1263149 with
// ThreadPool::FlushForTesting(), stalling thread pool tasks that need main
// thread collaboration.
TEST_F(TaskEnvironmentTest, MainThreadCanContributeWhileFlushingPool) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  base::RunLoop run_loop;
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
  TestWaitableEvent wait_for_collaboration;
  ThreadPool::PostTask(FROM_HERE, BindLambdaForTesting([&] {
                         task_environment.GetMainThreadTaskRunner()->PostTask(
                             FROM_HERE,
                             BindOnce(&TestWaitableEvent::Signal,
                                      Unretained(&wait_for_collaboration)));
                         wait_for_collaboration.Wait();
                       }));

  run_loop.Run();
}

TEST_F(TaskEnvironmentTest, CancelPendingTask) {
  TaskEnvironment task_environment(
      TaskEnvironment::TimeSource::MOCK_TIME,
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  CancelableOnceClosure task1(BindOnce([] {}));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, task1.callback(), Seconds(1));
  EXPECT_TRUE(task_environment.MainThreadIsIdle());
  EXPECT_EQ(1u, task_environment.GetPendingMainThreadTaskCount());
  EXPECT_EQ(Seconds(1), task_environment.NextMainThreadPendingTaskDelay());
  EXPECT_TRUE(task_environment.MainThreadIsIdle());
  task1.Cancel();
  EXPECT_TRUE(task_environment.MainThreadIsIdle());
  EXPECT_EQ(TimeDelta::Max(),
            task_environment.NextMainThreadPendingTaskDelay());

  CancelableRepeatingClosure task2(BindRepeating([] {}));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, task2.callback(), Seconds(1));
  task2.Cancel();
  EXPECT_EQ(0u, task_environment.GetPendingMainThreadTaskCount());

  CancelableRepeatingClosure task3(BindRepeating([] {}));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, task3.callback(), Seconds(1));
  task3.Cancel();
  EXPECT_EQ(TimeDelta::Max(),
            task_environment.NextMainThreadPendingTaskDelay());

  CancelableRepeatingClosure task4(BindRepeating([] {}));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, task4.callback(), Seconds(1));
  task4.Cancel();
  EXPECT_TRUE(task_environment.MainThreadIsIdle());
}

TEST_F(TaskEnvironmentTest, CancelPendingImmediateTask) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);
  EXPECT_TRUE(task_environment.MainThreadIsIdle());

  CancelableOnceClosure task1(BindOnce([] {}));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                        task1.callback());
  EXPECT_FALSE(task_environment.MainThreadIsIdle());

  task1.Cancel();
  EXPECT_TRUE(task_environment.MainThreadIsIdle());
}

TEST_F(TaskEnvironmentTest, NoFastForwardToCancelledTask) {
  TaskEnvironment task_environment(
      TaskEnvironment::TimeSource::MOCK_TIME,
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  TimeTicks start_time = task_environment.NowTicks();
  CancelableRepeatingClosure task(BindRepeating([] {}));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, task.callback(), Seconds(1));
  EXPECT_EQ(Seconds(1), task_environment.NextMainThreadPendingTaskDelay());
  task.Cancel();
  task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(start_time, task_environment.NowTicks());
}

TEST_F(TaskEnvironmentTest, NextTaskIsDelayed) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  EXPECT_FALSE(task_environment.NextTaskIsDelayed());
  CancelableRepeatingClosure task(BindRepeating([] {}));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, task.callback(), Seconds(1));
  EXPECT_TRUE(task_environment.NextTaskIsDelayed());
  task.Cancel();
  EXPECT_FALSE(task_environment.NextTaskIsDelayed());

  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, BindOnce([] {}), Seconds(2));
  EXPECT_TRUE(task_environment.NextTaskIsDelayed());
  task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(task_environment.NextTaskIsDelayed());

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                        BindOnce([] {}));
  EXPECT_FALSE(task_environment.NextTaskIsDelayed());
}

TEST_F(TaskEnvironmentTest, NextMainThreadPendingTaskDelayWithImmediateTask) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  EXPECT_EQ(TimeDelta::Max(),
            task_environment.NextMainThreadPendingTaskDelay());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                        BindOnce([] {}));
  EXPECT_EQ(TimeDelta(), task_environment.NextMainThreadPendingTaskDelay());
}

TEST_F(TaskEnvironmentTest, TimeSourceMockTimeAlsoMocksNow) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  const TimeTicks start_ticks = task_environment.NowTicks();
  EXPECT_EQ(TimeTicks::Now(), start_ticks);

  const Time start_time = Time::Now();

  const LiveTicks start_live_ticks = task_environment.NowLiveTicks();
  EXPECT_EQ(LiveTicks::Now(), start_live_ticks);

  constexpr TimeDelta kDelay = Seconds(10);
  task_environment.FastForwardBy(kDelay);
  EXPECT_EQ(TimeTicks::Now(), start_ticks + kDelay);
  EXPECT_EQ(Time::Now(), start_time + kDelay);
  EXPECT_EQ(LiveTicks::Now(), start_live_ticks + kDelay);
}

TEST_F(TaskEnvironmentTest, SingleThread) {
  SingleThreadTaskEnvironment task_environment;
  EXPECT_THAT(ThreadPoolInstance::Get(), IsNull());

  bool ran = false;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] { ran = true; }));
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(ran);

  EXPECT_DCHECK_DEATH(ThreadPool::PostTask(FROM_HERE, {}, DoNothing()));
}

// Verify that traits other than ThreadingMode can be applied to
// SingleThreadTaskEnvironment.
TEST_F(TaskEnvironmentTest, SingleThreadMockTime) {
  SingleThreadTaskEnvironment task_environment(
      TaskEnvironment::TimeSource::MOCK_TIME);

  const TimeTicks start_time = TimeTicks::Now();

  constexpr TimeDelta kDelay = Seconds(100);

  int counter = 0;
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([&] { counter += 1; }), kDelay);
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] { counter += 2; }));

  int expected_value = 0;
  EXPECT_EQ(expected_value, counter);

  task_environment.RunUntilIdle();
  expected_value += 2;
  EXPECT_EQ(expected_value, counter);

  task_environment.FastForwardUntilNoTasksRemain();
  expected_value += 1;
  EXPECT_EQ(expected_value, counter);
  EXPECT_EQ(TimeTicks::Now(), start_time + kDelay);
}

#if BUILDFLAG(IS_WIN)
namespace {

enum class ApartmentType {
  kSTA,
  kMTA,
};

void InitializeSTAApartment() {
  base::win::ScopedCOMInitializer initializer;
  EXPECT_TRUE(initializer.Succeeded());
}

void InitializeMTAApartment() {
  base::win::ScopedCOMInitializer initializer(
      base::win::ScopedCOMInitializer::kMTA);
  EXPECT_TRUE(initializer.Succeeded());
}

void InitializeCOMOnWorker(
    TaskEnvironment::ThreadPoolCOMEnvironment com_environment,
    ApartmentType apartment_type) {
  TaskEnvironment task_environment(com_environment);
  ThreadPool::PostTask(FROM_HERE, BindOnce(apartment_type == ApartmentType::kSTA
                                               ? &InitializeSTAApartment
                                               : &InitializeMTAApartment));
  task_environment.RunUntilIdle();
}

}  // namespace

TEST_F(TaskEnvironmentTest, DefaultCOMEnvironment) {
  // Attempt to initialize an MTA COM apartment. Expect this to succeed since
  // the thread is already in an MTA apartment.
  InitializeCOMOnWorker(TaskEnvironment::ThreadPoolCOMEnvironment::DEFAULT,
                        ApartmentType::kMTA);

  // Attempt to initialize an STA COM apartment. Expect this to fail since the
  // thread is already in an MTA apartment.
  EXPECT_DCHECK_DEATH(InitializeCOMOnWorker(
      TaskEnvironment::ThreadPoolCOMEnvironment::DEFAULT, ApartmentType::kSTA));
}

TEST_F(TaskEnvironmentTest, NoCOMEnvironment) {
  // Attempt to initialize both MTA and STA COM apartments. Both should succeed
  // when the thread is not already in an apartment.
  InitializeCOMOnWorker(TaskEnvironment::ThreadPoolCOMEnvironment::NONE,
                        ApartmentType::kMTA);
  InitializeCOMOnWorker(TaskEnvironment::ThreadPoolCOMEnvironment::NONE,
                        ApartmentType::kSTA);
}
#endif  // BUILDFLAG(IS_WIN)

// TODO(crbug.com/40835641): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
#define MAYBE_ParallelExecutionFence DISABLED_ParallelExecutionFence
#else
#define MAYBE_ParallelExecutionFence ParallelExecutionFence
#endif
TEST_F(TaskEnvironmentTest, MAYBE_ParallelExecutionFence) {
  TaskEnvironment task_environment;

  constexpr int kNumParallelTasks =
      TaskEnvironment::kNumForegroundThreadPoolThreads;

  TestWaitableEvent resume_main_thread;
  TestWaitableEvent all_runs_done;
  // Counters, all accessed/modified with memory_order_relaxed as no memory
  // ordering is necessary between operations.
  std::atomic_int completed_runs{0};
  std::atomic_int next_run{1};

  // Each task will repost itself until run 500. Run #50 will signal
  // |resume_main_thread|.
  RepeatingClosure task = BindLambdaForTesting([&] {
    int this_run = next_run.fetch_add(1, std::memory_order_relaxed);

    if (this_run == 50) {
      resume_main_thread.Signal();
    }

    // Sleep after signaling to increase the likelihood the main thread installs
    // the fence during this run and must wait on this task.
    if (this_run >= 50 && this_run < 50 + kNumParallelTasks) {
      PlatformThread::Sleep(Milliseconds(5));
    }

    // Repost self until the last kNumParallelTasks.
    if (this_run <= 500 - kNumParallelTasks) {
      ThreadPool::PostTask(task);
    }

    completed_runs.fetch_add(1, std::memory_order_relaxed);

    if (this_run == 500) {
      all_runs_done.Signal();
    }
  });
  for (int i = 0; i < kNumParallelTasks; ++i) {
    ThreadPool::PostTask(task);
  }

  resume_main_thread.Wait();
  ASSERT_GE(next_run.load(std::memory_order_relaxed), 50);

  {
    // Confirm that no run happens while the fence is up.
    TaskEnvironment::ParallelExecutionFence fence;

    // All runs are complete.
    const int completed_runs1 = completed_runs.load(std::memory_order_relaxed);
    const int next_run1 = next_run.load(std::memory_order_relaxed);
    EXPECT_EQ(completed_runs1, next_run1 - 1);

    // Given a bit more time, no additional run starts nor completes.
    PlatformThread::Sleep(Milliseconds(30));
    const int completed_runs2 = completed_runs.load(std::memory_order_relaxed);
    const int next_run2 = next_run.load(std::memory_order_relaxed);
    EXPECT_EQ(completed_runs1, completed_runs2);
    EXPECT_EQ(next_run1, next_run2);
  }

  // Runs resume automatically after taking down the fence (without needing to
  // call RunUntilIdle()).
  all_runs_done.Wait();
  ASSERT_EQ(completed_runs.load(std::memory_order_relaxed), 500);
  ASSERT_EQ(next_run.load(std::memory_order_relaxed), 501);
}

TEST_F(TaskEnvironmentTest, ParallelExecutionFenceWithoutTaskEnvironment) {
  // Noops (doesn't crash) without a TaskEnvironment.
  TaskEnvironment::ParallelExecutionFence fence;
}

TEST_F(TaskEnvironmentTest,
       ParallelExecutionFenceWithSingleThreadTaskEnvironment) {
  SingleThreadTaskEnvironment task_environment;
  // Noops (doesn't crash), with a SingleThreadTaskEnvironment/
  TaskEnvironment::ParallelExecutionFence fence;
}

// Android doesn't support death tests, see base/test/gtest_util.h
#if !BUILDFLAG(IS_ANDROID)
TEST_F(TaskEnvironmentTest, ParallelExecutionFenceNonMainThreadDeath) {
  TaskEnvironment task_environment;

  ThreadPool::PostTask(BindOnce([] {
#if CHECK_WILL_STREAM()
    const char kFailureLog[] = "ParallelExecutionFence invoked from worker";
#else
    const char kFailureLog[] = "";
#endif
    EXPECT_DEATH_IF_SUPPORTED(
        { TaskEnvironment::ParallelExecutionFence fence(kFailureLog); },
        kFailureLog);
  }));

  task_environment.RunUntilIdle();
}
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {
bool FailOnTaskEnvironmentLog(int severity,
                              const char* file,
                              int line,
                              size_t message_start,
                              const std::string& str) {
  std::string_view file_str(file);
  if (file_str.find("task_environment.cc") != std::string_view::npos) {
    ADD_FAILURE() << str;
    return true;
  }
  return false;
}
}  // namespace

// Regression test for crbug.com/1293931
TEST_F(TaskEnvironmentTest, DisallowRunTasksRetriesForFullTimeout) {
  TaskEnvironment task_environment;

  // Verify that steps below can let 1 second pass without generating logs.
  auto previous_handler = logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(&FailOnTaskEnvironmentLog);

  TestWaitableEvent worker_running;
  TestWaitableEvent resume_worker_task;

  ThreadPool::PostTask(BindLambdaForTesting([&] {
    worker_running.Signal();
    resume_worker_task.Wait();
  }));

  // Churn on this task so that TestTaskTracker::task_completed_cv_ gets
  // signaled a bunch and reproduces the bug's conditions
  // (TestTaskTracker::DisallowRunTasks gets early chances to quit).
  RepeatingClosure infinite_repost = BindLambdaForTesting([&] {
    if (!resume_worker_task.IsSignaled()) {
      ThreadPool::PostTask(infinite_repost);
    }
  });
  ThreadPool::PostTask(infinite_repost);

  // Allow ThreadPool quiescence after 1 second of test.
  ThreadPool::PostDelayedTask(
      FROM_HERE,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&resume_worker_task)),
      Seconds(1));

  worker_running.Wait();
  {
    // Attempt to instantiate a ParallelExecutionFence. Without the fix to
    // crbug.com/1293931, this would result in quickly exiting DisallowRunTasks
    // without waiting for the intended 5 seconds timeout and would emit
    // erroneous WARNING logs about slow tasks. This test passses if it doesn't
    // trip FailOnTaskEnvironmentLog().
    TaskEnvironment::ParallelExecutionFence fence;
  }

  // Flush the last |infinite_repost| task to avoid a UAF on
  // |resume_worker_task|.
  task_environment.RunUntilIdle();

  logging::SetLogMessageHandler(previous_handler);
}

TEST_F(TaskEnvironmentTest, RunUntilQuit_RunsMainThread) {
  TaskEnvironment task_environment;
  bool task_run = false;
  auto quit = task_environment.QuitClosure();

  SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                     BindLambdaForTesting([&] {
                                                       task_run = true;
                                                       quit.Run();
                                                     }));
  task_environment.RunUntilQuit();

  ASSERT_TRUE(task_run);
}

TEST_F(TaskEnvironmentTest, RunUntilQuit_RunsThreadPool) {
  TaskEnvironment task_environment;
  bool task_run = false;
  auto quit = task_environment.QuitClosure();

  ThreadPool::PostTask(FROM_HERE, BindLambdaForTesting([&] {
                         task_run = true;
                         quit.Run();
                       }));
  task_environment.RunUntilQuit();

  ASSERT_TRUE(task_run);
}

namespace {

class TestLogger {
 public:
  std::vector<std::string> GetLog() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return log_;
  }

  void LogMessage(std::string s) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    log_.push_back(std::move(s));
  }

  // If n=0 then executes `done` and returns. Otherwise adds `n` to the log and
  // reschedules itself with (n - 1).
  void CountDown(int n, OnceClosure done) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (n == 0) {
      std::move(done).Run();
      return;
    }

    log_.push_back(NumberToString(n));

    SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(&TestLogger::CountDown, Unretained(this), n - 1,
                            std::move(done)));
  }

 private:
  std::vector<std::string> log_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

TEST_F(TaskEnvironmentTest, RunUntilQuit_QueuedExecution) {
  TaskEnvironment task_environment(
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  SequenceBound<TestLogger> logger(ThreadPool::CreateSequencedTaskRunner({}));
  logger.AsyncCall(&TestLogger::CountDown)
      .WithArgs(5, task_environment.QuitClosure());
  // Because `task_environment` was created with
  // ThreadPoolExecutionMode::QUEUED, we are guaranteed that LogMessage() will
  // be called after the first run on CountDown() and before the rest.
  logger.AsyncCall(&TestLogger::LogMessage).WithArgs("Test");
  task_environment.RunUntilQuit();

  // Get the log and confirm that LogMessage() ran when expected.
  std::vector<std::string> actual_log;
  auto quit = task_environment.QuitClosure();
  logger.AsyncCall(&TestLogger::GetLog)
      .Then(BindLambdaForTesting([&](std::vector<std::string> log) {
        actual_log = log;
        quit.Run();
      }));
  task_environment.RunUntilQuit();

  ASSERT_THAT(actual_log,
              testing::ElementsAre("5", "Test", "4", "3", "2", "1"));
}

TEST_F(TaskEnvironmentTest, RunUntilQuit_ThreadPoolStaysQueued) {
  TaskEnvironment task_environment(
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  ThreadPool::PostTask(FROM_HERE, task_environment.QuitClosure());
  task_environment.RunUntilQuit();

  // RunUntilQuit() let the thread pool execute until the quit closure was run.
  // Verify that execution is now queued again.

  bool task_run = false;
  ThreadPool::PostTask(FROM_HERE,
                       BindLambdaForTesting([&] { task_run = true; }));
  // Wait a little bit to let the task run if execution is not queued.
  PlatformThread::Sleep(Milliseconds(10));

  ASSERT_FALSE(task_run);

  // Run the queued task now (if we don't, it'll run when `task_environment` is
  // destroyed, and `task_run` is out of scope).
  task_environment.RunUntilIdle();
}

TEST_F(TaskEnvironmentTest, RunUntilQuit_QuitClosureInvalidatedByRun) {
  TaskEnvironment task_environment(
      TaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  auto quit1 = task_environment.QuitClosure();
  auto quit2 = task_environment.QuitClosure();
  quit1.Run();
  task_environment.RunUntilQuit();  // Invalidates `quit1` and `quit2`.
  auto quit3 = task_environment.QuitClosure();

  std::vector<std::string> log;
  // Running `quit1` or `quit2` will have no effect.
  SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE, quit1);
  SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE, quit2);
  // This line will be logged.
  SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] { log.push_back("after quit2"); }));
  // `quit3` will terminate execution.
  SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE, quit3);
  // This line will *not* be logged.
  SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] { log.push_back("after quit3"); }));
  task_environment.RunUntilQuit();

  ASSERT_THAT(log, testing::ElementsAre("after quit2"));

  // Run the queued task now (if we don't, it might run when `task_environment`
  // is destroyed, and `log` is out of scope).
  task_environment.RunUntilIdle();
}

TEST_F(TaskEnvironmentTest, RunUntilQuit_MustCallQuitClosureFirst) {
  TaskEnvironment task_environment;
  EXPECT_DCHECK_DEATH_WITH(
      task_environment.RunUntilQuit(),
      R"(QuitClosure\(\) not called before RunUntilQuit\(\))");
}

}  // namespace test
}  // namespace base
