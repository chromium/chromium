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
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/types/pass_key.h"

namespace base {

namespace {

using SequenceManagerImpl = sequence_manager::internal::SequenceManagerImpl;

constinit thread_local SingleThreadTaskRunner::CurrentDefaultHandle*
    current_default_handle = nullptr;

constinit SingleThreadTaskRunner::MainThreadDefaultHandle*
    main_thread_default_handle = nullptr;

bool can_override = false;

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

// static
scoped_refptr<SingleThreadTaskRunner>
SingleThreadTaskRunner::GetCurrentBestEffort() {
  if (auto task_runner = SequenceManagerImpl::GetCurrentBestEffortTaskRunner(
          PassKey<SingleThreadTaskRunner>())) {
    return task_runner;
  }
  return GetCurrentDefault();
}

// static
bool SingleThreadTaskRunner::HasCurrentBestEffort() {
  return !!SequenceManagerImpl::GetCurrentBestEffortTaskRunner(
      PassKey<SingleThreadTaskRunner>());
}

// static
const scoped_refptr<SingleThreadTaskRunner>&
SingleThreadTaskRunner::GetMainThreadDefault() {
  const auto* const handle = main_thread_default_handle;
  CHECK(handle && handle->task_runner_)
      << "Error: The main thread's handle is not initialized yet. This "
         "probably means that you're calling this function too early in the "
         "process's lifetime. If you're in a test, you can use "
         "base::test::TaskEnvironement";
  return handle->task_runner_;
}

// static
bool SingleThreadTaskRunner::HasMainThreadDefault() {
  return !!main_thread_default_handle &&
         !!main_thread_default_handle->task_runner_;
}

// static
scoped_refptr<SingleThreadTaskRunner>
SingleThreadTaskRunner::GetMainThreadBestEffort() {
  if (HasMainThreadBestEffort()) {
    return main_thread_default_handle->best_effort_task_runner_;
  }
  return GetMainThreadDefault();
}

// static
bool SingleThreadTaskRunner::HasMainThreadBestEffort() {
  return !!main_thread_default_handle &&
         !!main_thread_default_handle->best_effort_task_runner_;
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

SingleThreadTaskRunner::MainThreadDefaultHandle::MainThreadDefaultHandle(
    scoped_refptr<SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      // `task_runner` belongs to this thread, so if there's a BEST_EFFORT task
      // runner for the thread GetCurrentBestEffortTaskRunner will return it.
      best_effort_task_runner_(
          SequenceManagerImpl::GetCurrentBestEffortTaskRunner(
              PassKey<SingleThreadTaskRunner>())),
      previous_handle_(main_thread_default_handle) {
  CHECK(!main_thread_default_handle || can_override);
  main_thread_default_handle = this;
}

SingleThreadTaskRunner::MainThreadDefaultHandle::~MainThreadDefaultHandle() {
  DCHECK_EQ(main_thread_default_handle, this);
  main_thread_default_handle = previous_handle_;
}

SingleThreadTaskRunner::ScopedCanOverrideMainThreadDefaultHandle::
    ScopedCanOverrideMainThreadDefaultHandle() {
  CHECK(!can_override);
  can_override = true;
}

SingleThreadTaskRunner::ScopedCanOverrideMainThreadDefaultHandle::
    ~ScopedCanOverrideMainThreadDefaultHandle() {
  CHECK(can_override);
  can_override = false;
}

}  // namespace base
