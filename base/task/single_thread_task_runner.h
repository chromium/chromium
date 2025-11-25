// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SINGLE_THREAD_TASK_RUNNER_H_
#define BASE_TASK_SINGLE_THREAD_TASK_RUNNER_H_

#include <optional>

#include "base/base_export.h"
#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/task/sequenced_task_runner.h"

namespace blink::scheduler {
class MainThreadSchedulerImpl;
}  // namespace blink::scheduler

namespace base::sequence_manager::internal {
class CurrentDefaultHandleOverrideForRunOrPostTask;
}

namespace base {

class ScopedDisallowRunningRunLoop;

// A SingleThreadTaskRunner is a SequencedTaskRunner with one more
// guarantee; namely, that all tasks are run on a single dedicated
// thread.  Most use cases require only a SequencedTaskRunner, unless
// there is a specific need to run tasks on only a single thread.
//
// SingleThreadTaskRunner implementations might:
//   - Post tasks to an existing thread's MessageLoop (see
//     MessageLoop::task_runner()).
//   - Create their own worker thread and MessageLoop to post tasks to.
//   - Add tasks to a FIFO and signal to a non-MessageLoop thread for them to
//     be processed. This allows TaskRunner-oriented code run on threads
//     running other kinds of message loop, e.g. Jingle threads.
class BASE_EXPORT SingleThreadTaskRunner : public SequencedTaskRunner {
 public:
  // Returns true if the `SingleThreadTaskRunner` runs tasks posted to it on the
  // current thread.
  //
  // Identical to `RunsTaskInCurrentSequence()`, except from a `RunOrPostTask()`
  // callback running synchronously (in that case, `BelongsToCurrentThread()`
  // returns false and `RunsTaskInCurrentSequence()` returns true).
  virtual bool BelongsToCurrentThread() const;

  // Returns the default SingleThreadTaskRunner for the current thread.
  // On threads that service multiple task queues, the default task queue is
  // preferred to inheriting the current task queue (otherwise, everything would
  // implicitly be "input priority"...). If the caller knows which task queue it
  // should be running on, it should post to that SingleThreadTaskRunner
  // directly instead of GetCurrentDefault(). This is critical in some
  // cases, e.g. DeleteSoon or RefCountedDeleteOnSequence should delete the
  // object on the same task queue it's used from (or on a lower priority).
  //
  // CHECKs if the current thread isn't servicing a SingleThreadTaskRunner.
  //
  // See
  // https://chromium.googlesource.com/chromium/src/+/main/docs/threading_and_tasks.md#Posting-to-the-Current-Virtual_Thread
  // for details
  [[nodiscard]] static const scoped_refptr<SingleThreadTaskRunner>&
  GetCurrentDefault();

  // Returns true if the SingleThreadTaskRunner is already created for
  // the current thread.
  [[nodiscard]] static bool HasCurrentDefault();

  // Returns a SingleThreadTaskRunner for the current thread. If possible, the
  // task runner will schedule tasks with BEST_EFFORT TaskPriority.
  // On threads that service multiple task queues, this returns the
  // lowest-priority task queue. On other threads, it returns the same value as
  // GetCurrentDefault().
  //
  // CHECKs if the current thread isn't servicing a SingleThreadTaskRunner.
  [[nodiscard]] static scoped_refptr<SingleThreadTaskRunner>
  GetCurrentBestEffort();

  // Returns true if the current thread supports multiple task queues, one of
  // which uses BEST_EFFORT TaskPriority.
  //
  // If this returns false and HasCurrentDefault() returns true,
  // GetCurrentBestEffort() will return the same value as GetCurrentDefault().
  [[nodiscard]] static bool HasCurrentBestEffort();

  // Returns the default SingleThreadTaskRunner for the main thread.
  //
  // CHECKs if the main thread task runner hasn't yet been initialized.
  [[nodiscard]] static const scoped_refptr<SingleThreadTaskRunner>&
  GetMainThreadDefault();

  // Returns true if the SingleThreadTaskRunner is already created for
  // the main thread.
  [[nodiscard]] static bool HasMainThreadDefault();

  // Returns a SingleThreadTaskRunner for the main thread. If possible, the
  // task runner will schedule tasks with BEST_EFFORT TaskPriority. If no
  // BEST_EFFORT task queue exists for the main threead, this returns the same
  // value is GetMainThreadDefault().
  //
  // CHECKs if the main thread task runner hasn't yet been initialized.
  [[nodiscard]] static scoped_refptr<SingleThreadTaskRunner>
  GetMainThreadBestEffort();

  // Returns true if the main thread has a BEST_EFFORT task queue.
  //
  // If this returns false and HasMainThreadDefault() returns true,
  // GetMainThreadBestEffort() will return the same value as
  // GetMainThreadDefault().
  [[nodiscard]] static bool HasMainThreadBestEffort();

  class CurrentHandleOverrideForTesting;

  class BASE_EXPORT CurrentDefaultHandle {
   public:
    // Sets the value returned by `SingleThreadTaskRunner::GetCurrentDefault()`
    // and `SequencedTaskRunner::GetCurrentDefault()` to `task_runner` within
    // its scope. `task_runner` must belong to the current thread. There must
    // not already be a current default `SingleThreadTaskRunner` on this thread.
    explicit CurrentDefaultHandle(
        scoped_refptr<SingleThreadTaskRunner> task_runner);

    CurrentDefaultHandle(const CurrentDefaultHandle&) = delete;
    CurrentDefaultHandle& operator=(const CurrentDefaultHandle&) = delete;

    ~CurrentDefaultHandle();

   private:
    friend class SingleThreadTaskRunner;

    // Overriding an existing current default SingleThreadTaskRunner should only
    // be needed under special circumstances. Require them to be enumerated as
    // friends to require //base/OWNERS review. Use
    // SingleThreadTaskRunner::CurrentHandleOverrideForTesting in unit tests to
    // avoid the friend requirement.
    friend class blink::scheduler::MainThreadSchedulerImpl;
    friend class CurrentHandleOverrideForTesting;
    friend class sequence_manager::internal::
        CurrentDefaultHandleOverrideForRunOrPostTask;
    FRIEND_TEST_ALL_PREFIXES(SingleThreadTaskRunnerCurrentDefaultHandleTest,
                             NestedRunLoopAllowedUnderHandleOverride);
    FRIEND_TEST_ALL_PREFIXES(SingleThreadTaskRunnerCurrentDefaultHandleTest,
                             NestedOverrideWithMayAlreadyExist);
    FRIEND_TEST_ALL_PREFIXES(SingleThreadTaskRunnerCurrentDefaultHandleTest,
                             OverrideWithNull);
    FRIEND_TEST_ALL_PREFIXES(SingleThreadTaskRunnerCurrentDefaultHandleTest,
                             OverrideWithNonNull);

    struct MayAlreadyExist {};

    // Same as the public constructor, but there may already be a current
    // default `SingleThreadTaskRunner` on this thread.
    CurrentDefaultHandle(scoped_refptr<SingleThreadTaskRunner> task_runner,
                         MayAlreadyExist);

    scoped_refptr<SingleThreadTaskRunner> task_runner_;
    // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of
    // speedometer3).
    RAW_PTR_EXCLUSION CurrentDefaultHandle* previous_handle_ = nullptr;
    SequencedTaskRunner::CurrentDefaultHandle sequenced_handle_;
  };

  // Overrides the current default `SingleThreadTaskRunner` and disables running
  // a `RunLoop` within its scope.
  //
  // Note: Overriding the current default SingleThreadTaskRunner isn't generally
  // desired but it's useful in some unit tests where multiple task runners
  // share the main thread for simplicity and determinism. Only use this when no
  // other constructs will work (see base/test/task_environment.h and
  // base/test/test_mock_time_task_runner.h for preferred alternatives).
  class BASE_EXPORT CurrentHandleOverrideForTesting {
   public:
    explicit CurrentHandleOverrideForTesting(
        scoped_refptr<SingleThreadTaskRunner> overriding_task_runner);
    ~CurrentHandleOverrideForTesting();

   private:
    CurrentDefaultHandle current_default_handle_;
    std::unique_ptr<ScopedDisallowRunningRunLoop> no_running_during_override_;
  };

  class BASE_EXPORT MainThreadDefaultHandle {
   public:
    // Sets the value returned by
    // `SingleThreadTaskRunner::GetMainThreadDefault()` to `task_runner` within
    // its scope. `task_runner` must belong to the current thread. There must
    // not already be a current default `SingleThreadTaskRunner` on this thread.
    // For tests where this is necessary, it's possible to use
    // ScopedCanOverrideMainThreadDefaultHandle.
    explicit MainThreadDefaultHandle(
        scoped_refptr<SingleThreadTaskRunner> task_runner);

    ~MainThreadDefaultHandle();

   private:
    friend class SingleThreadTaskRunner;

    scoped_refptr<SingleThreadTaskRunner> task_runner_;

    // An associated BEST_EFFORT task runner, or nullptr if there is none.
    scoped_refptr<SingleThreadTaskRunner> best_effort_task_runner_;

    // Some tests requires the ability to override the `previous_handle_`.
    // TODO(pmonette): Remove this when this is no longer the case.
    raw_ptr<MainThreadDefaultHandle> previous_handle_ = nullptr;
  };

  // Allows overriding the main thread default handle in some test
  // configuration. Callers must be friended to avoid spreading its usage.
  class BASE_EXPORT ScopedCanOverrideMainThreadDefaultHandle {
   private:
    friend class ScopedMockTimeMessageLoopTaskRunner;

    ScopedCanOverrideMainThreadDefaultHandle();
    ~ScopedCanOverrideMainThreadDefaultHandle();
  };

 protected:
  ~SingleThreadTaskRunner() override = default;
};

}  // namespace base

#endif  // BASE_TASK_SINGLE_THREAD_TASK_RUNNER_H_
