// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/post_job.h"

#include "base/task/scoped_set_task_priority_for_current_thread.h"
#include "base/task/thread_pool/job_task_source.h"
#include "base/task/thread_pool/pooled_task_runner_delegate.h"
#include "base/task/thread_pool/thread_pool_impl.h"
#include "base/task/thread_pool/thread_pool_instance.h"

namespace base {

namespace {

scoped_refptr<internal::JobTaskSource> CreateJobTaskSource(
    const Location& from_here,
    const TaskTraits& traits,
    RepeatingCallback<void(JobDelegate*)> worker_task,
    MaxConcurrencyCallback max_concurrency_callback) {
  DCHECK(ThreadPoolInstance::Get())
      << "Hint: if this is in a unit test, you're likely merely missing a "
         "base::test::TaskEnvironment member in your fixture.\n";

  return base::MakeRefCounted<internal::JobTaskSource>(
      from_here, traits, std::move(worker_task),
      std::move(max_concurrency_callback),
      static_cast<internal::ThreadPoolImpl*>(ThreadPoolInstance::Get()));
}

}  // namespace

JobDelegate::JobDelegate(
    internal::JobTaskSource* task_source,
    internal::PooledTaskRunnerDelegate* pooled_task_runner_delegate)
    : task_source_(task_source),
      pooled_task_runner_delegate_(pooled_task_runner_delegate) {
  DCHECK(task_source_);
}

JobDelegate::~JobDelegate() {
  if (task_id_ != kInvalidTaskId)
    task_source_->ReleaseTaskId(task_id_);
}

bool JobDelegate::ShouldYield() {
#if DCHECK_IS_ON()
  // ShouldYield() shouldn't be called again after returning true.
  DCHECK(!last_should_yield_);
#endif  // DCHECK_IS_ON()
  const bool should_yield =
      task_source_->ShouldYield() ||
      (pooled_task_runner_delegate_ &&
       pooled_task_runner_delegate_->ShouldYield(task_source_));

#if DCHECK_IS_ON()
  last_should_yield_ = should_yield;
#endif  // DCHECK_IS_ON()
  return should_yield;
}

void JobDelegate::YieldIfNeeded() {
  // TODO(crbug.com/40574605): Implement this.
}

void JobDelegate::NotifyConcurrencyIncrease() {
  task_source_->NotifyConcurrencyIncrease();
}

uint8_t JobDelegate::GetTaskId() {
  if (task_id_ == kInvalidTaskId)
    task_id_ = task_source_->AcquireTaskId();
  return task_id_;
}

JobHandle::JobHandle() = default;

JobHandle::JobHandle(scoped_refptr<internal::JobTaskSource> task_source)
    : task_source_(std::move(task_source)) {}

JobHandle::~JobHandle() {
  DCHECK(!task_source_)
      << "The Job must be cancelled, detached or joined before its "
         "JobHandle is destroyed.";
}

JobHandle::JobHandle(JobHandle&&) = default;

JobHandle& JobHandle::operator=(JobHandle&& other) {
  DCHECK(!task_source_)
      << "The Job must be cancelled, detached or joined before its "
         "JobHandle is re-assigned.";
  task_source_ = std::move(other.task_source_);
  return *this;
}

bool JobHandle::IsActive() const {
  return task_source_->IsActive();
}

void JobHandle::UpdatePriority(TaskPriority new_priority) {
  if (!internal::PooledTaskRunnerDelegate::MatchesCurrentDelegate(
          task_source_->delegate())) {
    return;
  }
  task_source_->delegate()->UpdateJobPriority(task_source_, new_priority);
}

void JobHandle::NotifyConcurrencyIncrease() {
  if (!internal::PooledTaskRunnerDelegate::MatchesCurrentDelegate(
          task_source_->delegate())) {
    return;
  }
  task_source_->NotifyConcurrencyIncrease();
}

void JobHandle::Join() {
  DCHECK(internal::PooledTaskRunnerDelegate::MatchesCurrentDelegate(
      task_source_->delegate()));
  DCHECK_GE(internal::GetTaskPriorityForCurrentThread(),
            task_source_->priority_racy())
      << "Join may not be called on Job with higher priority than the current "
         "thread.";
  UpdatePriority(internal::GetTaskPriorityForCurrentThread());
  if (task_source_->GetRemainingConcurrency() != 0) {
    // Make sure the task source is in the queue if not enough workers are
    // contributing. This is necessary for CreateJob(...).Join(). This is a
    // noop if the task source was already in the queue.
    task_source_->delegate()->EnqueueJobTaskSource(task_source_);
  }
  bool must_run = task_source_->WillJoin();
  while (must_run)
    must_run = task_source_->RunJoinTask();
  // Remove |task_source_| from the ThreadPool to prevent access to
  // |max_concurrency_callback| after Join().
  task_source_->delegate()->RemoveJobTaskSource(task_source_);
  task_source_ = nullptr;
}

void JobHandle::Cancel() {
  DCHECK(internal::PooledTaskRunnerDelegate::MatchesCurrentDelegate(
      task_source_->delegate()));
  task_source_->Cancel();
  bool must_run = task_source_->WillJoin();
  DCHECK(!must_run);
  // Remove |task_source_| from the ThreadPool to prevent access to
  // |max_concurrency_callback| after Join().
  task_source_->delegate()->RemoveJobTaskSource(task_source_);
  task_source_ = nullptr;
}

void JobHandle::CancelAndDetach() {
  task_source_->Cancel();
  Detach();
}

void JobHandle::Detach() {
  DCHECK(task_source_);
  task_source_ = nullptr;
}

JobHandle PostJob(const Location& from_here,
                  const TaskTraits& traits,
                  RepeatingCallback<void(JobDelegate*)> worker_task,
                  MaxConcurrencyCallback max_concurrency_callback) {
  auto task_source =
      CreateJobTaskSource(from_here, traits, std::move(worker_task),
                          std::move(max_concurrency_callback));
  const bool queued =
      static_cast<internal::ThreadPoolImpl*>(ThreadPoolInstance::Get())
          ->EnqueueJobTaskSource(task_source);
  if (queued) {
    return internal::JobTaskSource::CreateJobHandle(std::move(task_source));
  }
  return JobHandle();
}

JobHandle CreateJob(const Location& from_here,
                    const TaskTraits& traits,
                    RepeatingCallback<void(JobDelegate*)> worker_task,
                    MaxConcurrencyCallback max_concurrency_callback) {
  auto task_source =
      CreateJobTaskSource(from_here, traits, std::move(worker_task),
                          std::move(max_concurrency_callback));
  return internal::JobTaskSource::CreateJobHandle(std::move(task_source));
}

}  // namespace base
