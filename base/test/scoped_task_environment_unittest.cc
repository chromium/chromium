// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_task_environment.h"

#include <memory>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <unistd.h>
#include "base/files/file_descriptor_watcher_posix.h"
#endif  // defined(OS_POSIX)

namespace base {
namespace test {

namespace {

class ScopedTaskEnvironmentTest
    : public testing::TestWithParam<ScopedTaskEnvironment::MainThreadType> {};

void VerifyRunUntilIdleDidNotReturnAndSetFlag(
    AtomicFlag* run_until_idle_returned,
    AtomicFlag* task_ran) {
  EXPECT_FALSE(run_until_idle_returned->IsSet());
  task_ran->Set();
}

void RunUntilIdleTest(
    ScopedTaskEnvironment::MainThreadType main_thread_type,
    ScopedTaskEnvironment::ExecutionMode execution_control_mode) {
  AtomicFlag run_until_idle_returned;
  ScopedTaskEnvironment scoped_task_environment(main_thread_type,
                                                execution_control_mode);

  AtomicFlag first_main_thread_task_ran;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                          Unretained(&run_until_idle_returned),
                          Unretained(&first_main_thread_task_ran)));

  AtomicFlag first_task_scheduler_task_ran;
  PostTask(FROM_HERE, BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                               Unretained(&run_until_idle_returned),
                               Unretained(&first_task_scheduler_task_ran)));

  AtomicFlag second_task_scheduler_task_ran;
  AtomicFlag second_main_thread_task_ran;
  PostTaskAndReply(FROM_HERE,
                   BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                            Unretained(&run_until_idle_returned),
                            Unretained(&second_task_scheduler_task_ran)),
                   BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                            Unretained(&run_until_idle_returned),
                            Unretained(&second_main_thread_task_ran)));

  scoped_task_environment.RunUntilIdle();
  run_until_idle_returned.Set();

  EXPECT_TRUE(first_main_thread_task_ran.IsSet());
  EXPECT_TRUE(first_task_scheduler_task_ran.IsSet());
  EXPECT_TRUE(second_task_scheduler_task_ran.IsSet());
  EXPECT_TRUE(second_main_thread_task_ran.IsSet());
}

}  // namespace

TEST_P(ScopedTaskEnvironmentTest, QueuedRunUntilIdle) {
  RunUntilIdleTest(GetParam(), ScopedTaskEnvironment::ExecutionMode::QUEUED);
}

TEST_P(ScopedTaskEnvironmentTest, AsyncRunUntilIdle) {
  RunUntilIdleTest(GetParam(), ScopedTaskEnvironment::ExecutionMode::ASYNC);
}

// Verify that tasks posted to an ExecutionMode::QUEUED ScopedTaskEnvironment do
// not run outside of RunUntilIdle().
TEST_P(ScopedTaskEnvironmentTest, QueuedTasksDoNotRunOutsideOfRunUntilIdle) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ExecutionMode::QUEUED);

  AtomicFlag run_until_idle_called;
  PostTask(FROM_HERE, BindOnce(
                          [](AtomicFlag* run_until_idle_called) {
                            EXPECT_TRUE(run_until_idle_called->IsSet());
                          },
                          Unretained(&run_until_idle_called)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  run_until_idle_called.Set();
  scoped_task_environment.RunUntilIdle();

  AtomicFlag other_run_until_idle_called;
  PostTask(FROM_HERE, BindOnce(
                          [](AtomicFlag* other_run_until_idle_called) {
                            EXPECT_TRUE(other_run_until_idle_called->IsSet());
                          },
                          Unretained(&other_run_until_idle_called)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  other_run_until_idle_called.Set();
  scoped_task_environment.RunUntilIdle();
}

// Verify that a task posted to an ExecutionMode::ASYNC ScopedTaskEnvironment
// can run without a call to RunUntilIdle().
TEST_P(ScopedTaskEnvironmentTest, AsyncTasksRunAsTheyArePosted) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ExecutionMode::ASYNC);

  WaitableEvent task_ran(WaitableEvent::ResetPolicy::MANUAL,
                         WaitableEvent::InitialState::NOT_SIGNALED);
  PostTask(FROM_HERE,
           BindOnce([](WaitableEvent* task_ran) { task_ran->Signal(); },
                    Unretained(&task_ran)));
  task_ran.Wait();
}

// Verify that a task posted to an ExecutionMode::ASYNC ScopedTaskEnvironment
// after a call to RunUntilIdle() can run without another call to
// RunUntilIdle().
TEST_P(ScopedTaskEnvironmentTest,
       AsyncTasksRunAsTheyArePostedAfterRunUntilIdle) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ExecutionMode::ASYNC);

  scoped_task_environment.RunUntilIdle();

  WaitableEvent task_ran(WaitableEvent::ResetPolicy::MANUAL,
                         WaitableEvent::InitialState::NOT_SIGNALED);
  PostTask(FROM_HERE,
           BindOnce([](WaitableEvent* task_ran) { task_ran->Signal(); },
                    Unretained(&task_ran)));
  task_ran.Wait();
}

TEST_P(ScopedTaskEnvironmentTest, DelayedTasks) {
  // Use a QUEUED execution-mode environment, so that no tasks are actually
  // executed until RunUntilIdle()/FastForwardBy() are invoked.
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ExecutionMode::QUEUED);

  subtle::Atomic32 counter = 0;

  constexpr base::TimeDelta kShortTaskDelay = TimeDelta::FromDays(1);
  // Should run only in MOCK_TIME environment when time is fast-forwarded.
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      Bind(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 4);
          },
          Unretained(&counter)),
      kShortTaskDelay);
  // TODO(gab): This currently doesn't run because the TaskScheduler's clock
  // isn't mocked but it should be.
  PostDelayedTask(FROM_HERE,
                  Bind(
                      [](subtle::Atomic32* counter) {
                        subtle::NoBarrier_AtomicIncrement(counter, 128);
                      },
                      Unretained(&counter)),
                  kShortTaskDelay);

  constexpr base::TimeDelta kLongTaskDelay = TimeDelta::FromDays(7);
  // Same as first task, longer delays to exercise
  // FastForwardUntilNoTasksRemain().
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      Bind(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 8);
          },
          Unretained(&counter)),
      TimeDelta::FromDays(5));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      Bind(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 16);
          },
          Unretained(&counter)),
      kLongTaskDelay);

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, Bind(
                     [](subtle::Atomic32* counter) {
                       subtle::NoBarrier_AtomicIncrement(counter, 1);
                     },
                     Unretained(&counter)));
  PostTask(FROM_HERE, Bind(
                          [](subtle::Atomic32* counter) {
                            subtle::NoBarrier_AtomicIncrement(counter, 2);
                          },
                          Unretained(&counter)));

  // This expectation will fail flakily if the preceding PostTask() is executed
  // asynchronously, indicating a problem with the QUEUED execution mode.
  int expected_value = 0;
  EXPECT_EQ(expected_value, counter);

  // RunUntilIdle() should process non-delayed tasks only in all queues.
  scoped_task_environment.RunUntilIdle();
  expected_value += 1;
  expected_value += 2;
  EXPECT_EQ(expected_value, counter);

  if (GetParam() == ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {
    // Delay inferior to the delay of the first posted task.
    constexpr base::TimeDelta kInferiorTaskDelay = TimeDelta::FromSeconds(1);
    static_assert(kInferiorTaskDelay < kShortTaskDelay,
                  "|kInferiorTaskDelay| should be "
                  "set to a value inferior to the first posted task's delay.");
    scoped_task_environment.FastForwardBy(kInferiorTaskDelay);
    EXPECT_EQ(expected_value, counter);

    scoped_task_environment.FastForwardBy(kShortTaskDelay - kInferiorTaskDelay);
    expected_value += 4;
    EXPECT_EQ(expected_value, counter);

    scoped_task_environment.FastForwardUntilNoTasksRemain();
    expected_value += 8;
    expected_value += 16;
    EXPECT_EQ(expected_value, counter);
  }
}

// Regression test for https://crbug.com/824770.
TEST_P(ScopedTaskEnvironmentTest, SupportsSequenceLocalStorageOnMainThread) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ExecutionMode::ASYNC);

  SequenceLocalStorageSlot<int> sls_slot;
  sls_slot.Set(5);
  EXPECT_EQ(5, sls_slot.Get());
}

#if defined(OS_POSIX)
TEST_F(ScopedTaskEnvironmentTest, SupportsFileDescriptorWatcherOnIOMainThread) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::IO,
      ScopedTaskEnvironment::ExecutionMode::ASYNC);

  int pipe_fds_[2];
  ASSERT_EQ(0, pipe(pipe_fds_));

  RunLoop run_loop;

  // The write end of a newly created pipe is immediately writable.
  auto controller = FileDescriptorWatcher::WatchWritable(
      pipe_fds_[1], run_loop.QuitClosure());

  // This will hang if the notification doesn't occur as expected.
  run_loop.Run();
}
#endif  // defined(OS_POSIX)

// Verify that the TickClock returned by
// |ScopedTaskEnvironment::GetMockTickClock| gets updated when the
// FastForward(By|UntilNoTasksRemain) functions are called.
TEST_F(ScopedTaskEnvironmentTest, FastForwardAdvanceTickClock) {
  // Use a QUEUED execution-mode environment, so that no tasks are actually
  // executed until RunUntilIdle()/FastForwardBy() are invoked.
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
      ScopedTaskEnvironment::ExecutionMode::QUEUED);

  constexpr base::TimeDelta kShortTaskDelay = TimeDelta::FromDays(1);
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, base::DoNothing(),
                                                 kShortTaskDelay);

  constexpr base::TimeDelta kLongTaskDelay = TimeDelta::FromDays(7);
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, base::DoNothing(),
                                                 kLongTaskDelay);

  const base::TickClock* tick_clock =
      scoped_task_environment.GetMockTickClock();
  base::TimeTicks tick_clock_ref = tick_clock->NowTicks();

  // Make sure that |FastForwardBy| advances the clock.
  scoped_task_environment.FastForwardBy(kShortTaskDelay);
  EXPECT_EQ(kShortTaskDelay, tick_clock->NowTicks() - tick_clock_ref);

  // Make sure that |FastForwardUntilNoTasksRemain| advances the clock.
  scoped_task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(kLongTaskDelay, tick_clock->NowTicks() - tick_clock_ref);

  // Fast-forwarding to a time at which there's no tasks should also advance the
  // clock.
  scoped_task_environment.FastForwardBy(kLongTaskDelay);
  EXPECT_EQ(kLongTaskDelay * 2, tick_clock->NowTicks() - tick_clock_ref);
}

namespace {

class MockLifetimeObserver : public ScopedTaskEnvironment::LifetimeObserver {
 public:
  MockLifetimeObserver() = default;
  ~MockLifetimeObserver() override = default;

  MOCK_METHOD2(OnScopedTaskEnvironmentCreated,
               void(ScopedTaskEnvironment::MainThreadType,
                    scoped_refptr<SingleThreadTaskRunner>));
  MOCK_METHOD0(OnScopedTaskEnvironmentDestroyed, void());
};

}  // namespace

TEST_F(ScopedTaskEnvironmentTest, LifetimeObserver) {
  testing::StrictMock<MockLifetimeObserver> lifetime_observer;
  ScopedTaskEnvironment::SetLifetimeObserver(&lifetime_observer);

  EXPECT_CALL(lifetime_observer,
              OnScopedTaskEnvironmentCreated(testing::_, testing::_));
  std::unique_ptr<ScopedTaskEnvironment> task_environment(
      std::make_unique<ScopedTaskEnvironment>());
  testing::Mock::VerifyAndClearExpectations(&lifetime_observer);

  EXPECT_CALL(lifetime_observer, OnScopedTaskEnvironmentDestroyed());
  task_environment.reset();
  testing::Mock::VerifyAndClearExpectations(&lifetime_observer);
}

INSTANTIATE_TEST_CASE_P(
    MainThreadDefault,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::DEFAULT));
INSTANTIATE_TEST_CASE_P(
    MainThreadMockTime,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::MOCK_TIME));
INSTANTIATE_TEST_CASE_P(
    MainThreadUI,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::UI));
INSTANTIATE_TEST_CASE_P(
    MainThreadIO,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::IO));

}  // namespace test
}  // namespace base
