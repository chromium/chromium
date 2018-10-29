// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue.h"

#include <utility>

#include "base/bind.h"
#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/task_queue_proxy.h"
#include "base/task/sequence_manager/task_queue_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace base {
namespace sequence_manager {

namespace {

// TODO(kraynov): Move NullTaskRunner from //base/test to //base.
scoped_refptr<SingleThreadTaskRunner> CreateNullTaskRunner() {
  return MakeRefCounted<internal::TaskQueueTaskRunner>(
      MakeRefCounted<internal::TaskQueueProxy>(
          nullptr, MakeRefCounted<internal::AssociatedThreadId>()),
      kTaskTypeNone);
}

}  // namespace

TaskQueue::TaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl,
                     const TaskQueue::Spec& spec)
    : impl_(std::move(impl)),
      sequence_manager_(impl_ ? impl_->GetSequenceManagerWeakPtr() : nullptr),
      associated_thread_((impl_ && impl_->sequence_manager())
                             ? impl_->sequence_manager()->associated_thread()
                             : MakeRefCounted<internal::AssociatedThreadId>()),
      default_task_runner_(impl_ ? impl_->CreateTaskRunner(kTaskTypeNone)
                                 : CreateNullTaskRunner()) {}

TaskQueue::~TaskQueue() {
  // scoped_refptr guarantees us that this object isn't used.
  if (!impl_)
    return;
  if (impl_->IsUnregistered())
    return;

  // If we've not been unregistered then this must occur on the main thread.
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  impl_->sequence_manager()->ShutdownTaskQueueGracefully(TakeTaskQueueImpl());
}

TaskQueue::TaskTiming::TaskTiming(bool has_wall_time, bool has_thread_time)
    : has_wall_time_(has_wall_time), has_thread_time_(has_thread_time) {}

void TaskQueue::TaskTiming::RecordTaskStart(LazyNow* now) {
  if (has_wall_time())
    start_time_ = now->Now();
  if (has_thread_time())
    start_thread_time_ = base::ThreadTicks::Now();
}

void TaskQueue::TaskTiming::RecordTaskEnd(LazyNow* now) {
  if (has_wall_time())
    end_time_ = now->Now();
  if (has_thread_time())
    end_thread_time_ = base::ThreadTicks::Now();
}

void TaskQueue::ShutdownTaskQueue() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  AutoLock lock(impl_lock_);
  if (!impl_)
    return;
  if (!sequence_manager_) {
    impl_.reset();
    return;
  }
  impl_->SetBlameContext(nullptr);
  impl_->SetOnTaskStartedHandler(
      internal::TaskQueueImpl::OnTaskStartedHandler());
  impl_->SetOnTaskCompletedHandler(
      internal::TaskQueueImpl::OnTaskCompletedHandler());
  sequence_manager_->UnregisterTaskQueueImpl(TakeTaskQueueImpl());
}

scoped_refptr<SingleThreadTaskRunner> TaskQueue::CreateTaskRunner(
    int task_type) {
  Optional<MoveableAutoLock> lock(AcquireImplReadLockIfNeeded());
  if (!impl_)
    return CreateNullTaskRunner();
  return impl_->CreateTaskRunner(task_type);
}

std::unique_ptr<TaskQueue::QueueEnabledVoter>
TaskQueue::CreateQueueEnabledVoter() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return nullptr;
  return impl_->CreateQueueEnabledVoter(this);
}

bool TaskQueue::IsQueueEnabled() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return false;
  return impl_->IsQueueEnabled();
}

bool TaskQueue::IsEmpty() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return true;
  return impl_->IsEmpty();
}

size_t TaskQueue::GetNumberOfPendingTasks() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return 0;
  return impl_->GetNumberOfPendingTasks();
}

bool TaskQueue::HasTaskToRunImmediately() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return false;
  return impl_->HasTaskToRunImmediately();
}

Optional<TimeTicks> TaskQueue::GetNextScheduledWakeUp() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return nullopt;
  return impl_->GetNextScheduledWakeUp();
}

void TaskQueue::SetQueuePriority(TaskQueue::QueuePriority priority) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return;
  impl_->SetQueuePriority(priority);
}

TaskQueue::QueuePriority TaskQueue::GetQueuePriority() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return TaskQueue::QueuePriority::kLowPriority;
  return impl_->GetQueuePriority();
}

void TaskQueue::AddTaskObserver(MessageLoop::TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return;
  impl_->AddTaskObserver(task_observer);
}

void TaskQueue::RemoveTaskObserver(MessageLoop::TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return;
  impl_->RemoveTaskObserver(task_observer);
}

void TaskQueue::SetTimeDomain(TimeDomain* time_domain) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return;
  impl_->SetTimeDomain(time_domain);
}

TimeDomain* TaskQueue::GetTimeDomain() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return nullptr;
  return impl_->GetTimeDomain();
}

void TaskQueue::SetBlameContext(trace_event::BlameContext* blame_context) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return;
  impl_->SetBlameContext(blame_context);
}

void TaskQueue::InsertFence(InsertFencePosition position) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return;
  impl_->InsertFence(position);
}

void TaskQueue::InsertFenceAt(TimeTicks time) {
  impl_->InsertFenceAt(time);
}

void TaskQueue::RemoveFence() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return;
  impl_->RemoveFence();
}

bool TaskQueue::HasActiveFence() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return false;
  return impl_->HasActiveFence();
}

bool TaskQueue::BlockedByFence() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return false;
  return impl_->BlockedByFence();
}

const char* TaskQueue::GetName() const {
  auto lock = AcquireImplReadLockIfNeeded();
  if (!impl_)
    return "";
  return impl_->GetName();
}

void TaskQueue::SetObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!impl_)
    return;
  if (observer) {
    // Observer is guaranteed to outlive TaskQueue and TaskQueueImpl lifecycle
    // is controlled by |this|.
    impl_->SetOnNextWakeUpChangedCallback(
        BindRepeating(&TaskQueue::Observer::OnQueueNextWakeUpChanged,
                      Unretained(observer), Unretained(this)));
  } else {
    impl_->SetOnNextWakeUpChangedCallback(RepeatingCallback<void(TimeTicks)>());
  }
}

bool TaskQueue::IsOnMainThread() const {
  return associated_thread_->thread_id == PlatformThread::CurrentId();
}

Optional<MoveableAutoLock> TaskQueue::AcquireImplReadLockIfNeeded() const {
  if (IsOnMainThread())
    return nullopt;
  return MoveableAutoLock(impl_lock_);
}

std::unique_ptr<internal::TaskQueueImpl> TaskQueue::TakeTaskQueueImpl() {
  DCHECK(impl_);
  return std::move(impl_);
}

}  // namespace sequence_manager
}  // namespace base
