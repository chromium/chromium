// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/deferred_sequenced_task_runner.h"
#include "base/task/common/scoped_defer_task_posting.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"

namespace base {

DeferredSequencedTaskRunner::DeferredTask::DeferredTask()
    : is_non_nestable(false) {
}

DeferredSequencedTaskRunner::DeferredTask::DeferredTask(DeferredTask&& other) =
    default;

DeferredSequencedTaskRunner::DeferredTask::~DeferredTask() = default;

DeferredSequencedTaskRunner::DeferredTask&
DeferredSequencedTaskRunner::DeferredTask::operator=(DeferredTask&& other) =
    default;

DeferredSequencedTaskRunner::DeferredSequencedTaskRunner(
    scoped_refptr<SequencedTaskRunner> target_task_runner)
    : created_thread_id_(PlatformThread::CurrentId()),
      target_task_runner_(std::move(target_task_runner)) {
#if DCHECK_IS_ON()
  AutoLock lock(lock_);
  DCHECK(target_task_runner_);
#endif
  task_runner_atomic_ptr_.store(target_task_runner_.get(),
                                std::memory_order_release);
}

DeferredSequencedTaskRunner::DeferredSequencedTaskRunner()
    : created_thread_id_(PlatformThread::CurrentId()) {}

bool DeferredSequencedTaskRunner::PostDelayedTask(const Location& from_here,
                                                  OnceClosure task,
                                                  TimeDelta delay) {
  // Do not process new PostTasks while we are handling a PostTask (tracing
  // has to do this) as it can lead to a deadlock and defer it instead.
  ScopedDeferTaskPosting disallow_task_posting;

  AutoLock lock(lock_);
  if (started_) {
    DCHECK(deferred_tasks_queue_.empty());
    return target_task_runner_->PostDelayedTask(from_here, std::move(task),
                                                delay);
  }

  QueueDeferredTask(from_here, std::move(task), delay,
                    false /* is_non_nestable */);
  return true;
}

bool DeferredSequencedTaskRunner::RunsTasksInCurrentSequence() const {
  // task_runner_atomic_ptr_ cannot change once it has been initialized, so it's
  // safe to access it without lock.
  SequencedTaskRunner* task_runner_ptr =
      task_runner_atomic_ptr_.load(std::memory_order_acquire);
  if (task_runner_ptr) {
    return task_runner_ptr->RunsTasksInCurrentSequence();
  }

  return created_thread_id_ == PlatformThread::CurrentId();
}

bool DeferredSequencedTaskRunner::PostNonNestableDelayedTask(
    const Location& from_here,
    OnceClosure task,
    TimeDelta delay) {
  AutoLock lock(lock_);
  if (started_) {
    DCHECK(deferred_tasks_queue_.empty());
    return target_task_runner_->PostNonNestableDelayedTask(
        from_here, std::move(task), delay);
  }
  QueueDeferredTask(from_here, std::move(task), delay,
                    true /* is_non_nestable */);
  return true;
}

void DeferredSequencedTaskRunner::Start() {
  AutoLock lock(lock_);
  StartImpl();
}

void DeferredSequencedTaskRunner::StartWithTaskRunner(
    scoped_refptr<SequencedTaskRunner> target_task_runner) {
  AutoLock lock(lock_);
  DCHECK(!target_task_runner_);
  DCHECK(target_task_runner);
  target_task_runner_ = std::move(target_task_runner);
  task_runner_atomic_ptr_.store(target_task_runner_.get(),
                                std::memory_order_release);
  StartImpl();
}

bool DeferredSequencedTaskRunner::Started() const {
  AutoLock lock(lock_);
  return started_;
}

DeferredSequencedTaskRunner::~DeferredSequencedTaskRunner() = default;

void DeferredSequencedTaskRunner::QueueDeferredTask(const Location& from_here,
                                                    OnceClosure task,
                                                    TimeDelta delay,
                                                    bool is_non_nestable) {
  lock_.AssertAcquired();

  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task);

  DeferredTask deferred_task;
  deferred_task.posted_from = from_here;
  deferred_task.task = std::move(task);
  deferred_task.delay = delay;
  deferred_task.is_non_nestable = is_non_nestable;
  deferred_tasks_queue_.push_back(std::move(deferred_task));
}

void DeferredSequencedTaskRunner::StartImpl() {
  lock_.AssertAcquired();  // Callers should have grabbed the lock.
  DCHECK(!started_);
  started_ = true;
  DCHECK(target_task_runner_);
  for (auto& task : deferred_tasks_queue_) {
    if (task.is_non_nestable) {
      target_task_runner_->PostNonNestableDelayedTask(
          task.posted_from, std::move(task.task), task.delay);
    } else {
      target_task_runner_->PostDelayedTask(task.posted_from,
                                           std::move(task.task), task.delay);
    }
  }
  deferred_tasks_queue_.clear();
}

}  // namespace base
