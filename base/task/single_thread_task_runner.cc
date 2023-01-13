// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/threading/thread_local.h"

namespace base {

namespace {

ThreadLocalPointer<SingleThreadTaskRunner::CurrentDefaultHandle>&
CurrentDefaultHandleTls() {
  static NoDestructor<
      ThreadLocalPointer<SingleThreadTaskRunner::CurrentDefaultHandle>>
      instance;
  return *instance;
}

}  // namespace

// static
const scoped_refptr<SingleThreadTaskRunner>&
SingleThreadTaskRunner::GetCurrentDefault() {
  const SingleThreadTaskRunner::CurrentDefaultHandle* current_default =
      CurrentDefaultHandleTls().Get();
  CHECK(current_default)
      << "Error: This caller requires a single-threaded context (i.e. the "
         "current task needs to run from a SingleThreadTaskRunner). If you're "
         "in a test refer to //docs/threading_and_tasks_testing.md."
      << (SequencedTaskRunner::HasCurrentDefault()
              ? " Note: base::SequencedTaskRunner::GetCurrentDefault() "
                "is set; "
                "consider using it if the current task can run from a "
                "SequencedTaskRunner."
              : "");
  return current_default->task_runner_;
}

// static
bool SingleThreadTaskRunner::HasCurrentDefault() {
  return !!CurrentDefaultHandleTls().Get();
}

SingleThreadTaskRunner::CurrentDefaultHandle::CurrentDefaultHandle(
    scoped_refptr<SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      sequenced_task_runner_current_default_(task_runner_) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!CurrentDefaultHandleTls().Get());
  CurrentDefaultHandleTls().Set(this);
}

SingleThreadTaskRunner::CurrentDefaultHandle::~CurrentDefaultHandle() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(CurrentDefaultHandleTls().Get(), this);
  CurrentDefaultHandleTls().Set(nullptr);
}

SingleThreadTaskRunner::CurrentHandleOverride::CurrentHandleOverride(
    scoped_refptr<SingleThreadTaskRunner> overriding_task_runner,
    bool allow_nested_runloop) {
  DCHECK(!SequencedTaskRunner::HasCurrentDefault() ||
         SingleThreadTaskRunner::HasCurrentDefault())
      << "SingleThreadTaskRunner::CurrentHandleOverride is not compatible with "
         "a SequencedTaskRunner::CurrentDefaultHandle already "
         "being set on this thread (except when it's "
         "set by the current "
         "SingleThreadTaskRunner::CurrentDefaultHandle).";

  if (!SingleThreadTaskRunner::HasCurrentDefault()) {
    top_level_thread_task_runner_current_default_.emplace(
        std::move(overriding_task_runner));
    return;
  }

#if DCHECK_IS_ON()
  expected_task_runner_before_restore_ = overriding_task_runner.get();
#endif
  SingleThreadTaskRunner::CurrentDefaultHandle* current_default =
      CurrentDefaultHandleTls().Get();
  SequencedTaskRunner::SetCurrentDefaultHandleTaskRunner(
      current_default->sequenced_task_runner_current_default_,
      overriding_task_runner);
  current_default->task_runner_.swap(overriding_task_runner);
  // Due to the swap, now `current_default->task_runner_` points to the
  // overriding task runner and `overriding_task_runner_` points to the previous
  // task runner.
  task_runner_to_restore_ = std::move(overriding_task_runner);

  if (!allow_nested_runloop) {
    no_running_during_override_ =
        std::make_unique<ScopedDisallowRunningRunLoop>();
  }
}

SingleThreadTaskRunner::CurrentHandleOverride::~CurrentHandleOverride() {
  if (task_runner_to_restore_) {
    SingleThreadTaskRunner::CurrentDefaultHandle* current_default =
        CurrentDefaultHandleTls().Get();

#if DCHECK_IS_ON()
    DCHECK_EQ(expected_task_runner_before_restore_,
              current_default->task_runner_.get())
        << "Nested overrides must expire their "
           "SingleThreadTaskRunner::CurrentHandleOverride "
           "in LIFO order.";
#endif

    SequencedTaskRunner::SetCurrentDefaultHandleTaskRunner(
        current_default->sequenced_task_runner_current_default_,
        task_runner_to_restore_);
    current_default->task_runner_.swap(task_runner_to_restore_);
  }
}

}  // namespace base
