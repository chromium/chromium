// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_THREAD_TASK_RUNNER_HANDLE_H_
#define BASE_THREADING_THREAD_TASK_RUNNER_HANDLE_H_

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {
namespace scheduler {
class MainThreadSchedulerImpl;
}  // namespace scheduler
}  // namespace blink

namespace base {

// ThreadTaskRunnerHandle stores a reference to a thread's TaskRunner
// in thread-local storage.  Callers can then retrieve the TaskRunner
// for the current thread by calling ThreadTaskRunnerHandle::Get().
// At most one TaskRunner may be bound to each thread at a time.
// Prefer SequencedTaskRunnerHandle to this unless thread affinity is required.
class BASE_EXPORT ThreadTaskRunnerHandle {
 public:
  // Gets the SingleThreadTaskRunner for the current thread.
  static const scoped_refptr<SingleThreadTaskRunner>& Get() WARN_UNUSED_RESULT;

  // Returns true if the SingleThreadTaskRunner is already created for
  // the current thread.
  static bool IsSet() WARN_UNUSED_RESULT;

  // Binds |task_runner| to the current thread. |task_runner| must belong
  // to the current thread for this to succeed.
  explicit ThreadTaskRunnerHandle(
      scoped_refptr<SingleThreadTaskRunner> task_runner);
  ~ThreadTaskRunnerHandle();

 private:
  friend class ThreadTaskRunnerHandleOverride;
  scoped_refptr<SingleThreadTaskRunner> task_runner_;

  // Registers |task_runner_|'s SequencedTaskRunner interface as the
  // SequencedTaskRunnerHandle on this thread.
  SequencedTaskRunnerHandle sequenced_task_runner_handle_;

  DISALLOW_COPY_AND_ASSIGN(ThreadTaskRunnerHandle);
};

// ThreadTaskRunnerHandleOverride overrides the task runner returned by
// |ThreadTaskRunnerHandle::Get()| to point at |overriding_task_runner| until
// the |ThreadTaskRunnerHandleOverride| goes out of scope.
// ThreadTaskRunnerHandleOverride instantiates a new ThreadTaskRunnerHandle if
// ThreadTaskRunnerHandle is not instantiated on the current thread. Nested
// overrides are allowed but callers must ensure the
// |ThreadTaskRunnerHandleOverride| expire in LIFO (stack) order.
//
// Note: nesting ThreadTaskRunnerHandle is subtle and should be done with care,
// hence the need to friend and request a //base/OWNERS review for usage outside
// of tests. Use ThreadTaskRunnerHandleOverrideForTesting to bypass the friend
// requirement in tests.
class BASE_EXPORT ThreadTaskRunnerHandleOverride {
 public:
  ThreadTaskRunnerHandleOverride(const ThreadTaskRunnerHandleOverride&) =
      delete;
  ThreadTaskRunnerHandleOverride& operator=(
      const ThreadTaskRunnerHandleOverride&) = delete;
  ~ThreadTaskRunnerHandleOverride();

 private:
  friend class ThreadTaskRunnerHandleOverrideForTesting;
  FRIEND_TEST_ALL_PREFIXES(ThreadTaskRunnerHandleTest, NestedRunLoop);

  // We expect ThreadTaskRunnerHandleOverride to be only needed under special
  // circumstances. Require them to be enumerated as friends to require
  // //base/OWNERS review. Use ThreadTaskRunnerHandleOverrideForTesting
  // in unit tests to avoid the friend requirement.

  friend class blink::scheduler::MainThreadSchedulerImpl;

  // Constructs a ThreadTaskRunnerHandleOverride which will make
  // ThreadTaskRunnerHandle::Get() return |overriding_task_runner| for its
  // lifetime. |allow_nested_loop| specifies whether RunLoop::Run() is allowed
  // during this override's lifetime. It's not recommended to allow this unless
  // the current thread's scheduler guarantees that only tasks which pertain to
  // |overriding_task_runner|'s context will be run by nested RunLoops.
  explicit ThreadTaskRunnerHandleOverride(
      scoped_refptr<SingleThreadTaskRunner> overriding_task_runner,
      bool allow_nested_runloop = false);

  absl::optional<ThreadTaskRunnerHandle> top_level_thread_task_runner_handle_;
  scoped_refptr<SingleThreadTaskRunner> task_runner_to_restore_;
#if DCHECK_IS_ON()
  SingleThreadTaskRunner* expected_task_runner_before_restore_{nullptr};
#endif
  absl::optional<RunLoop::ScopedDisallowRunning> no_running_during_override_;
};

// Note: nesting ThreadTaskRunnerHandles isn't generally desired but it's useful
// in some unit tests where multiple task runners share the main thread for
// simplicity and determinism. Only use this when no other constructs will work
// (see base/test/task_environment.h and base/test/test_mock_time_task_runner.h
// for preferred alternatives).
class ThreadTaskRunnerHandleOverrideForTesting {
 public:
  explicit ThreadTaskRunnerHandleOverrideForTesting(
      scoped_refptr<SingleThreadTaskRunner> overriding_task_runner)
      : thread_task_runner_handle_override_(std::move(overriding_task_runner)) {
  }

 private:
  ThreadTaskRunnerHandleOverride thread_task_runner_handle_override_;
};

}  // namespace base

#endif  // BASE_THREADING_THREAD_TASK_RUNNER_HANDLE_H_
