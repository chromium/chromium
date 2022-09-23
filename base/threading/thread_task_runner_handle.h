// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_THREAD_TASK_RUNNER_HANDLE_H_
#define BASE_THREADING_THREAD_TASK_RUNNER_HANDLE_H_

#include "base/base_export.h"
#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

// ThreadTaskRunnerHandle stores a reference to a thread's TaskRunner
// in thread-local storage.  Callers can then retrieve the TaskRunner
// for the current thread by calling ThreadTaskRunnerHandle::Get().
// At most one TaskRunner may be bound to each thread at a time.
// Prefer SequencedTaskRunnerHandle to this unless thread affinity is required.
class BASE_EXPORT ThreadTaskRunnerHandle {
 public:
  // DEPRECATED: use SingleThreadTaskRunner::GetCurrentDefault instead
  // Gets the SingleThreadTaskRunner for the current thread.
  [[nodiscard]] static const scoped_refptr<SingleThreadTaskRunner>& Get();

  // DEPRECATED: Use SingleThreadTaskRunner::HasCurrentDefault
  // Returns true if the SingleThreadTaskRunner is already created for
  // the current thread.
  [[nodiscard]] static bool IsSet();

  // Binds |task_runner| to the current thread. |task_runner| must belong
  // to the current thread for this to succeed.
  explicit ThreadTaskRunnerHandle(
      scoped_refptr<SingleThreadTaskRunner> task_runner)
      : contained_current_default_(std::move(task_runner)) {}

  ThreadTaskRunnerHandle(const ThreadTaskRunnerHandle&) = delete;
  ThreadTaskRunnerHandle& operator=(const ThreadTaskRunnerHandle&) = delete;

  ~ThreadTaskRunnerHandle() = default;

 private:
  SingleThreadTaskRunner::CurrentDefaultHandle contained_current_default_;
};

// DEPRECATED: Use SingleThreadTaskRunner::CurrentHandleOverride instead.
//
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
  ~ThreadTaskRunnerHandleOverride() = default;

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
      bool allow_nested_runloop = false)
      : contained_override_(std::move(overriding_task_runner),
                            allow_nested_runloop) {}

  SingleThreadTaskRunner::CurrentHandleOverride contained_override_;
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
      : contained_override_(std::move(overriding_task_runner)) {}

  ~ThreadTaskRunnerHandleOverrideForTesting() = default;

 private:
  SingleThreadTaskRunner::CurrentHandleOverrideForTesting contained_override_;
};

}  // namespace base

#endif  // BASE_THREADING_THREAD_TASK_RUNNER_HANDLE_H_
