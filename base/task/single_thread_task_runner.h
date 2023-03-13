// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SINGLE_THREAD_TASK_RUNNER_H_
#define BASE_TASK_SINGLE_THREAD_TASK_RUNNER_H_

#include "base/auto_reset.h"
#include "base/base_export.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink::scheduler {
class MainThreadSchedulerImpl;
}  // namespace blink::scheduler

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
  // A more explicit alias to RunsTasksInCurrentSequence().
  bool BelongsToCurrentThread() const { return RunsTasksInCurrentSequence(); }

  // Returns the default SingleThreadTaskRunner for the current thread.
  // On threads that service multiple task queues, the default task queue is
  // preferred to inheriting the current task queue (otherwise, everything would
  // implicitly be "input priority"...). If the caller knows which task queue it
  // should be running on, it should post to that SingleThreadTaskRunner
  // directly instead of GetCurrentDefault(). This is critical in some
  // cases, e.g. DeleteSoon or RefCountedDeleteOnSequence should delete the
  // object on the same task queue it's used from (or on a lower priority).
  //
  // DCHECKs if the current thread isn't servicing a SingleThreadTaskRunner.
  //
  // See
  // https://chromium.googlesource.com/chromium/src/+/main/docs/threading_and_tasks.md#Posting-to-the-Current-Virtual_Thread
  // for details

  [[nodiscard]] static const scoped_refptr<SingleThreadTaskRunner>&
  GetCurrentDefault();

  // Returns true if the SingleThreadTaskRunner is already created for
  // the current thread.
  [[nodiscard]] static bool HasCurrentDefault();

  class CurrentHandleOverride;
  class CurrentHandleOverrideForTesting;

  class BASE_EXPORT CurrentDefaultHandle {
   public:
    // Binds |task_runner| to the current thread. |task_runner| must belong
    // to the current thread.
    explicit CurrentDefaultHandle(
        scoped_refptr<SingleThreadTaskRunner> task_runner);

    CurrentDefaultHandle(const CurrentDefaultHandle&) = delete;
    CurrentDefaultHandle& operator=(const CurrentDefaultHandle&) = delete;

    ~CurrentDefaultHandle();

   private:
    friend class SingleThreadTaskRunner;
    friend class CurrentHandleOverride;

    const AutoReset<CurrentDefaultHandle*> resetter_;

    scoped_refptr<SingleThreadTaskRunner> task_runner_;

    // Registers |task_runner_|'s SequencedTaskRunner interface as the
    // SequencedTaskRunner::CurrentDefaultHandle on this thread.
    SequencedTaskRunner::CurrentDefaultHandle
        sequenced_task_runner_current_default_;
  };

  // CurrentHandleOverride overrides the task runner returned by
  // |SingleThreadTaskRunner::GetCurrentDefault()| to point at
  // |overriding_task_runner| until the |CurrentHandleOverride| goes out of
  // scope. CurrentHandleOverride instantiates a new SingleThreadTaskRunner if
  // SingleThreadTaskRunner is not instantiated on the current thread. Nested
  // overrides are allowed but callers must ensure the |CurrentHandleOverride|s
  // expire in LIFO (stack) order.
  //
  // Note: nesting SingleThreadTaskRunner is subtle and should be done with
  // care, hence the need to friend and request a //base/OWNERS review for usage
  // outside of tests. Use CurrentHandleOverrideForTesting to bypass the friend
  // requirement in tests.
  class BASE_EXPORT CurrentHandleOverride {
   public:
    CurrentHandleOverride(const CurrentHandleOverride&) = delete;
    CurrentHandleOverride& operator=(const CurrentHandleOverride&) = delete;
    ~CurrentHandleOverride();

   private:
    friend class CurrentHandleOverrideForTesting;
    FRIEND_TEST_ALL_PREFIXES(SingleThreadTaskRunnerCurrentDefaultHandleTest,
                             NestedRunLoop);

    // We expect SingleThreadTaskRunner::CurrentHandleOverride to be only needed
    // under special circumstances. Require them to be enumerated as friends to
    // require //base/OWNERS review. Use
    // SingleTaskRunner::CurrentHandleOverrideForTesting in unit tests to avoid
    // the friend requirement.

    friend class blink::scheduler::MainThreadSchedulerImpl;

    // Constructs a SingleThreadTaskRunner::CurrentHandleOverride which will
    // make SingleThreadTaskRunner::GetCurrentDefault() return
    // |overriding_task_runner| for its lifetime. |allow_nested_loop| specifies
    // whether RunLoop::Run() is allowed during this override's lifetime. It's
    // not recommended to allow this unless the current thread's scheduler
    // guarantees that only tasks which pertain to |overriding_task_runner|'s
    // context will be run by nested RunLoops.
    explicit CurrentHandleOverride(
        scoped_refptr<SingleThreadTaskRunner> overriding_task_runner,
        bool allow_nested_runloop = false);

    absl::optional<SingleThreadTaskRunner::CurrentDefaultHandle>
        top_level_thread_task_runner_current_default_;

    scoped_refptr<SingleThreadTaskRunner> task_runner_to_restore_;

#if DCHECK_IS_ON()
    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #union
    RAW_PTR_EXCLUSION SingleThreadTaskRunner*
        expected_task_runner_before_restore_{nullptr};
#endif

    std::unique_ptr<ScopedDisallowRunningRunLoop> no_running_during_override_;
  };

  // Note: nesting CurrentHandleOverrides isn't generally desired but it's
  // useful in some unit tests where multiple task runners share the main thread
  // for simplicity and determinism. Only use this when no other constructs will
  // work (see base/test/task_environment.h and
  // base/test/test_mock_time_task_runner.h for preferred alternatives).
  class BASE_EXPORT CurrentHandleOverrideForTesting {
   public:
    explicit CurrentHandleOverrideForTesting(
        scoped_refptr<SingleThreadTaskRunner> overriding_task_runner)
        : thread_task_runner_current_override_(
              std::move(overriding_task_runner)) {}

   private:
    CurrentHandleOverride thread_task_runner_current_override_;
  };

 protected:
  ~SingleThreadTaskRunner() override = default;
};

}  // namespace base

#endif  // BASE_TASK_SINGLE_THREAD_TASK_RUNNER_H_
