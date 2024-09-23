// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/scoped_defer_task_posting.h"

#include "base/compiler_specific.h"

namespace base {

namespace {

// Holds a thread-local pointer to the current scope or null when no
// scope is active.
constinit thread_local ScopedDeferTaskPosting* scoped_defer_task_posting =
    nullptr;

}  // namespace

// static
void ScopedDeferTaskPosting::PostOrDefer(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const Location& from_here,
    OnceClosure task,
    base::TimeDelta delay) {
  ScopedDeferTaskPosting* scope = Get();
  if (scope) {
    scope->DeferTaskPosting(std::move(task_runner), from_here, std::move(task),
                            delay);
    return;
  }

  task_runner->PostDelayedTask(from_here, std::move(task), delay);
}

// static
ScopedDeferTaskPosting* ScopedDeferTaskPosting::Get() {
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&scoped_defer_task_posting, sizeof(ScopedDeferTaskPosting*));

  return scoped_defer_task_posting;
}

// static
bool ScopedDeferTaskPosting::Set(ScopedDeferTaskPosting* scope) {
  // We can post a task from within a ScheduleWork in some tests, so we can
  // get nested scopes. In this case ignore all except the top one.
  if (Get() && scope)
    return false;
  scoped_defer_task_posting = scope;
  return true;
}

// static
bool ScopedDeferTaskPosting::IsPresent() {
  return !!Get();
}

ScopedDeferTaskPosting::ScopedDeferTaskPosting() {
  top_level_scope_ = Set(this);
}

ScopedDeferTaskPosting::~ScopedDeferTaskPosting() {
  if (!top_level_scope_) {
    DCHECK(deferred_tasks_.empty());
    return;
  }
  Set(nullptr);
  for (DeferredTask& deferred_task : deferred_tasks_) {
    deferred_task.task_runner->PostDelayedTask(deferred_task.from_here,
                                               std::move(deferred_task.task),
                                               deferred_task.delay);
  }
}

ScopedDeferTaskPosting::DeferredTask::DeferredTask(
    scoped_refptr<SequencedTaskRunner> task_runner,
    Location from_here,
    OnceClosure task,
    base::TimeDelta delay)
    : task_runner(std::move(task_runner)),
      from_here(from_here),
      task(std::move(task)),
      delay(delay) {}

ScopedDeferTaskPosting::DeferredTask::DeferredTask(DeferredTask&&) = default;

ScopedDeferTaskPosting::DeferredTask::~DeferredTask() = default;

void ScopedDeferTaskPosting::DeferTaskPosting(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const Location& from_here,
    OnceClosure task,
    base::TimeDelta delay) {
  deferred_tasks_.push_back(
      {std::move(task_runner), from_here, std::move(task), delay});
}

}  // namespace base
