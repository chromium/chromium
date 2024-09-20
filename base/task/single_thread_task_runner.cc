// Copyright 2016 The Chromium Authors
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

namespace base {

namespace {

constinit thread_local SingleThreadTaskRunner::CurrentDefaultHandle*
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

bool SingleThreadTaskRunner::BelongsToCurrentThread() const {
  return RunsTasksInCurrentSequence();
}

// static
const scoped_refptr<SingleThreadTaskRunner>&
SingleThreadTaskRunner::GetCurrentDefault() {
  const auto* const handle = GetCurrentDefaultHandle();
  CHECK(handle && handle->task_runner_)
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
  return !!GetCurrentDefaultHandle() &&
         !!GetCurrentDefaultHandle()->task_runner_;
}

SingleThreadTaskRunner::CurrentDefaultHandle::CurrentDefaultHandle(
    scoped_refptr<SingleThreadTaskRunner> task_runner)
    : CurrentDefaultHandle(std::move(task_runner), MayAlreadyExist{}) {
  CHECK(!previous_handle_ || !previous_handle_->task_runner_);
}

SingleThreadTaskRunner::CurrentDefaultHandle::~CurrentDefaultHandle() {
  DCHECK_EQ(GetCurrentDefaultHandle(), this);
  current_default_handle = previous_handle_;
}

SingleThreadTaskRunner::CurrentDefaultHandle::CurrentDefaultHandle(
    scoped_refptr<SingleThreadTaskRunner> task_runner,
    MayAlreadyExist)
    : task_runner_(std::move(task_runner)),
      previous_handle_(GetCurrentDefaultHandle()),
      sequenced_handle_(
          task_runner_,
          SequencedTaskRunner::CurrentDefaultHandle::MayAlreadyExist{}) {
  // Support overriding the current default with a null task runner or a task
  // runner that belongs to the current thread.
  DCHECK(!task_runner_ || task_runner_->BelongsToCurrentThread());
  current_default_handle = this;
}

SingleThreadTaskRunner::CurrentHandleOverrideForTesting::
    CurrentHandleOverrideForTesting(
        scoped_refptr<SingleThreadTaskRunner> overriding_task_runner)
    : current_default_handle_(std::move(overriding_task_runner),
                              CurrentDefaultHandle::MayAlreadyExist{}),
      no_running_during_override_(
          std::make_unique<ScopedDisallowRunningRunLoop>()) {}

SingleThreadTaskRunner::CurrentHandleOverrideForTesting::
    ~CurrentHandleOverrideForTesting() = default;

}  // namespace base
