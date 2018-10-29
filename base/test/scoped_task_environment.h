// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_TASK_ENVIRONMENT_H_
#define BASE_TEST_SCOPED_TASK_ENVIRONMENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/task/lazy_task_runner.h"
#include "build/build_config.h"

namespace base {

namespace internal {
class ScopedSetSequenceLocalStorageMapForCurrentThread;
class SequenceLocalStorageMap;
}  // namespace internal

class FileDescriptorWatcher;
class MessageLoop;
class TaskScheduler;
class TestMockTimeTaskRunner;
class TickClock;

namespace test {

// ScopedTaskEnvironment allows usage of these APIs within its scope:
// - (Thread|Sequenced)TaskRunnerHandle, on the thread where it lives
// - base/task/post_task.h, on any thread
//
// Tests that need either of these APIs should instantiate a
// ScopedTaskEnvironment.
//
// Tasks posted to the (Thread|Sequenced)TaskRunnerHandle run synchronously when
// RunLoop::Run(UntilIdle) or ScopedTaskEnvironment::RunUntilIdle is called on
// the thread where the ScopedTaskEnvironment lives.
//
// Tasks posted through base/task/post_task.h run on dedicated threads. If
// ExecutionMode is QUEUED, they run when RunUntilIdle() or
// ~ScopedTaskEnvironment is called. If ExecutionMode is ASYNC, they run as they
// are posted.
//
// All methods of ScopedTaskEnvironment must be called from the same thread.
//
// Usage:
//
//   class MyTestFixture : public testing::Test {
//    public:
//     (...)
//
//    protected:
//     // Must be the first member (or at least before any member that cares
//     // about tasks) to be initialized first and destroyed last. protected
//     // instead of private visibility will allow controlling the task
//     // environment (e.g. clock) once such features are added (see design doc
//     // below for details), until then it at least doesn't hurt :).
//     base::test::ScopedTaskEnvironment scoped_task_environment_;
//
//     // Other members go here (or further below in private section.)
//   };
//
// Design and future improvements documented in
// https://docs.google.com/document/d/1QabRo8c7D9LsYY3cEcaPQbOCLo8Tu-6VLykYXyl3Pkk/edit
class ScopedTaskEnvironment {
 public:
  enum class MainThreadType {
    // The main thread doesn't pump system messages.
    DEFAULT,
    // The main thread doesn't pump system messages and uses a mock clock for
    // delayed tasks (controllable via FastForward*() methods).
    // TODO(gab): Make this the default |main_thread_type|.
    // TODO(gab): Also mock the TaskScheduler's clock simultaneously (this
    // currently only mocks the main thread's clock).
    MOCK_TIME,
    // The main thread pumps UI messages.
    UI,
    // The main thread pumps asynchronous IO messages and supports the
    // FileDescriptorWatcher API on POSIX.
    IO,
  };

  enum class ExecutionMode {
    // Tasks are queued and only executed when RunUntilIdle() is explicitly
    // called.
    QUEUED,
    // Tasks run as they are posted. RunUntilIdle() can still be used to block
    // until done.
    ASYNC,
  };

  ScopedTaskEnvironment(
      MainThreadType main_thread_type = MainThreadType::DEFAULT,
      ExecutionMode execution_control_mode = ExecutionMode::ASYNC);

  // Waits until no undelayed TaskScheduler tasks remain. Then, unregisters the
  // TaskScheduler and the (Thread|Sequenced)TaskRunnerHandle.
  ~ScopedTaskEnvironment();

  class LifetimeObserver {
   public:
    virtual ~LifetimeObserver() = default;

    virtual void OnScopedTaskEnvironmentCreated(
        MainThreadType main_thread_type,
        scoped_refptr<SingleThreadTaskRunner> task_runner) = 0;
    virtual void OnScopedTaskEnvironmentDestroyed() = 0;
  };

  // Set a thread-local observer which will get notifications when
  // a new ScopedTaskEnvironment is created or destroyed.
  // This is needed due to peculiarities of Blink initialisation
  // (Blink is per-test suite and ScopedTaskEnvironment is per-test).
  static void SetLifetimeObserver(LifetimeObserver* lifetime_observer);

  // Returns a TaskRunner that schedules tasks on the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> GetMainThreadTaskRunner();

  // Returns whether the main thread's TaskRunner has pending tasks.
  bool MainThreadHasPendingTask() const;

  // Runs tasks until both the (Thread|Sequenced)TaskRunnerHandle and the
  // TaskScheduler's non-delayed queues are empty.
  void RunUntilIdle();

  // Only valid for instances with a MOCK_TIME MainThreadType. Fast-forwards
  // virtual time by |delta|, causing all tasks on the main thread with a
  // remaining delay less than or equal to |delta| to be executed before this
  // returns. |delta| must be non-negative.
  // TODO(gab): Make this apply to TaskScheduler delayed tasks as well
  // (currently only main thread time is mocked).
  void FastForwardBy(TimeDelta delta);

  // Only valid for instances with a MOCK_TIME MainThreadType.
  // Short for FastForwardBy(TimeDelta::Max()).
  void FastForwardUntilNoTasksRemain();

  // Only valid for instances with a MOCK_TIME MainThreadType.  Returns a
  // TickClock whose time is updated by FastForward(By|UntilNoTasksRemain).
  const TickClock* GetMockTickClock();
  std::unique_ptr<TickClock> DeprecatedGetMockTickClock();

  // Only valid for instances with a MOCK_TIME MainThreadType.
  // Returns the current virtual tick time (initially starting at 0).
  base::TimeTicks NowTicks() const;

  // Only valid for instances with a MOCK_TIME MainThreadType.
  // Returns the number of pending tasks of the main thread's TaskRunner.
  size_t GetPendingMainThreadTaskCount() const;

  // Only valid for instances with a MOCK_TIME MainThreadType.
  // Returns the delay until the next delayed pending task of the main thread's
  // TaskRunner.
  TimeDelta NextMainThreadPendingTaskDelay() const;

 private:
  class TestTaskTracker;

  const ExecutionMode execution_control_mode_;

  // Exactly one of these will be non-null to provide the task environment on
  // the main thread. Users of this class should NOT rely on the presence of a
  // MessageLoop beyond (Thread|Sequenced)TaskRunnerHandle and RunLoop as
  // the backing implementation of each MainThreadType may change over time.
  const std::unique_ptr<MessageLoop> message_loop_;
  const scoped_refptr<TestMockTimeTaskRunner> mock_time_task_runner_;

  // Non-null in MOCK_TIME, where an explicit SequenceLocalStorageMap needs to
  // be provided. TODO(gab): This can be removed once mock time support is added
  // to MessageLoop directly.
  const std::unique_ptr<internal::SequenceLocalStorageMap> slsm_for_mock_time_;
  const std::unique_ptr<
      internal::ScopedSetSequenceLocalStorageMapForCurrentThread>
      slsm_registration_for_mock_time_;

#if defined(OS_POSIX)
  // Enables the FileDescriptorWatcher API iff running a MainThreadType::IO.
  const std::unique_ptr<FileDescriptorWatcher> file_descriptor_watcher_;
#endif

  const TaskScheduler* task_scheduler_ = nullptr;

  // Owned by |task_scheduler_|.
  TestTaskTracker* const task_tracker_;

  // Ensures destruction of lazy TaskRunners when this is destroyed.
  internal::ScopedLazyTaskRunnerListForTesting
      scoped_lazy_task_runner_list_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTaskEnvironment);
};

}  // namespace test
}  // namespace base

#endif  // BASE_TEST_SCOPED_ASYNC_TASK_SCHEDULER_H_
