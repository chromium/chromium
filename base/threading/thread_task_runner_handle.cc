// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_task_runner_handle.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/lazy_instance.h"
#include "base/run_loop.h"
#include "base/threading/thread_local.h"

namespace base {

namespace {

base::LazyInstance<base::ThreadLocalPointer<ThreadTaskRunnerHandle>>::Leaky
    thread_task_runner_tls = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
const scoped_refptr<SingleThreadTaskRunner>& ThreadTaskRunnerHandle::Get() {
  const ThreadTaskRunnerHandle* current =
      thread_task_runner_tls.Pointer()->Get();
  CHECK(current)
      << "Error: This caller requires a single-threaded context (i.e. the "
         "current task needs to run from a SingleThreadTaskRunner). If you're "
         "in a test refer to //docs/threading_and_tasks_testing.md.";
  return current->task_runner_;
}

// static
bool ThreadTaskRunnerHandle::IsSet() {
  return !!thread_task_runner_tls.Pointer()->Get();
}

ThreadTaskRunnerHandle::ThreadTaskRunnerHandle(
    scoped_refptr<SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      sequenced_task_runner_handle_(task_runner_) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!thread_task_runner_tls.Pointer()->Get());
  thread_task_runner_tls.Pointer()->Set(this);
}

ThreadTaskRunnerHandle::~ThreadTaskRunnerHandle() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(thread_task_runner_tls.Pointer()->Get(), this);
  thread_task_runner_tls.Pointer()->Set(nullptr);
}

ThreadTaskRunnerHandleOverride::ThreadTaskRunnerHandleOverride(
    scoped_refptr<SingleThreadTaskRunner> overriding_task_runner,
    bool allow_nested_runloop) {
  DCHECK(!SequencedTaskRunnerHandle::IsSet() || ThreadTaskRunnerHandle::IsSet())
      << "ThreadTaskRunnerHandleOverride is not compatible with a "
         "SequencedTaskRunnerHandle already being set on this thread (except "
         "when it's set by the current ThreadTaskRunnerHandle).";

  if (!ThreadTaskRunnerHandle::IsSet()) {
    top_level_thread_task_runner_handle_.emplace(
        std::move(overriding_task_runner));
    return;
  }

#if DCHECK_IS_ON()
  expected_task_runner_before_restore_ = overriding_task_runner.get();
#endif
  ThreadTaskRunnerHandle* ttrh = thread_task_runner_tls.Pointer()->Get();
  ttrh->sequenced_task_runner_handle_.task_runner_ = overriding_task_runner;
  ttrh->task_runner_.swap(overriding_task_runner);
  // Due to the swap, now `ttrh->task_runner_` points to the overriding task
  // runner and `overriding_task_runner_` points to the previous task runner.
  task_runner_to_restore_ = std::move(overriding_task_runner);

  if (!allow_nested_runloop)
    no_running_during_override_.emplace();
}

ThreadTaskRunnerHandleOverride::~ThreadTaskRunnerHandleOverride() {
  if (task_runner_to_restore_) {
    ThreadTaskRunnerHandle* ttrh = thread_task_runner_tls.Pointer()->Get();

#if DCHECK_IS_ON()
    DCHECK_EQ(expected_task_runner_before_restore_, ttrh->task_runner_.get())
        << "Nested overrides must expire their ThreadTaskRunnerHandleOverride "
           "in LIFO order.";
#endif

    ttrh->sequenced_task_runner_handle_.task_runner_ = task_runner_to_restore_;
    ttrh->task_runner_.swap(task_runner_to_restore_);
  }
}

}  // namespace base
