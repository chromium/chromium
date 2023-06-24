// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/run_loop.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {

namespace {

ABSL_CONST_INIT thread_local SingleThreadTaskRunner::CurrentDefaultHandle*
    current_default_handle = nullptr;

// This function can be removed, and the calls below replaced with direct
// variable accesses, once the MSAN workaround is not necessary.
SingleThreadTaskRunner::CurrentDefaultHandle* GetCurrentDefaultHandle() {
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&current_default_handle,
                sizeof(SingleThreadTaskRunner::CurrentDefaultHandle*));

  return current_default_handle;
}

}  // namespace

// static
const scoped_refptr<SingleThreadTaskRunner>&
SingleThreadTaskRunner::GetCurrentDefault() {
  const auto* const handle = GetCurrentDefaultHandle();
  CHECK(handle)
      << "Error: This caller requires a single-threaded context (i.e. the "
         "current task needs to run from a SingleThreadTaskRunner). If you're "
         "in a test refer to //docs/threading_and_tasks_testing.md."
      << (SequencedTaskRunner::HasCurrentDefault()
              ? " Note: base::SequencedTaskRunner::GetCurrentDefault() "
                "is set; "
                "consider using it if the current task can run from a "
                "SequencedTaskRunner."
              : "");
  return handle->task_runner_;
}

// static
bool SingleThreadTaskRunner::HasCurrentDefault() {
  return !!GetCurrentDefaultHandle();
}

SingleThreadTaskRunner::CurrentDefaultHandle::CurrentDefaultHandle(
    scoped_refptr<SingleThreadTaskRunner> task_runner)
    : resetter_(&current_default_handle, this, nullptr),
      task_runner_(std::move(task_runner)),
      sequenced_task_runner_current_default_(task_runner_) {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

SingleThreadTaskRunner::CurrentDefaultHandle::~CurrentDefaultHandle() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(GetCurrentDefaultHandle(), this);
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
  auto* const handle = GetCurrentDefaultHandle();
  SequencedTaskRunner::SetCurrentDefaultHandleTaskRunner(
      handle->sequenced_task_runner_current_default_, overriding_task_runner);
  handle->task_runner_.swap(overriding_task_runner);
  // Due to the swap, now `handle->task_runner_` points to the overriding task
  // runner and `overriding_task_runner_` points to the previous task runner.
  task_runner_to_restore_ = std::move(overriding_task_runner);

  if (!allow_nested_runloop) {
    no_running_during_override_ =
        std::make_unique<ScopedDisallowRunningRunLoop>();
  }
}

SingleThreadTaskRunner::CurrentHandleOverride::~CurrentHandleOverride() {
  if (task_runner_to_restore_) {
    auto* const handle = GetCurrentDefaultHandle();
#if DCHECK_IS_ON()
    DCHECK_EQ(expected_task_runner_before_restore_, handle->task_runner_.get())
        << "Nested overrides must expire their "
           "SingleThreadTaskRunner::CurrentHandleOverride "
           "in LIFO order.";
#endif

    SequencedTaskRunner::SetCurrentDefaultHandleTaskRunner(
        handle->sequenced_task_runner_current_default_,
        task_runner_to_restore_);
    handle->task_runner_.swap(task_runner_to_restore_);
  }
}

}  // namespace base
