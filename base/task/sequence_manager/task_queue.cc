// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_checker_impl.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace sequence_manager {

TaskQueue::QueueEnabledVoter::QueueEnabledVoter(
    WeakPtr<internal::TaskQueueImpl> task_queue)
    : task_queue_(std::move(task_queue)) {
  task_queue_->AddQueueEnabledVoter(enabled_, *this);
}

TaskQueue::QueueEnabledVoter::~QueueEnabledVoter() {
  if (task_queue_) {
    task_queue_->RemoveQueueEnabledVoter(enabled_, *this);
  }
}

void TaskQueue::QueueEnabledVoter::SetVoteToEnable(bool enabled) {
  if (enabled == enabled_) {
    return;
  }
  enabled_ = enabled;
  if (task_queue_) {
    task_queue_->OnQueueEnabledVoteChanged(enabled_);
  }
}

TaskQueue::TaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl,
                     const TaskQueue::Spec& spec)
    : impl_(std::move(impl)),
      sequence_manager_(impl_->GetSequenceManagerWeakPtr()),
      associated_thread_((impl_->sequence_manager())
                             ? impl_->sequence_manager()->associated_thread()
                             : MakeRefCounted<internal::AssociatedThreadId>()),
      default_task_runner_(impl_->CreateTaskRunner(kTaskTypeNone)),
      name_(impl_->GetProtoName()) {}

TaskQueue::~TaskQueue() {
  // scoped_refptr guarantees us that this object isn't used.
  if (!impl_)
    return;
  if (impl_->IsUnregistered())
    return;

  // If we've not been unregistered then this must occur on the main thread.
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  ShutdownTaskQueue();
}

TaskQueue::TaskTiming::TaskTiming(bool has_wall_time, bool has_thread_time)
    : has_wall_time_(has_wall_time), has_thread_time_(has_thread_time) {}

void TaskQueue::TaskTiming::RecordTaskStart(LazyNow* now) {
  DCHECK_EQ(State::NotStarted, state_);
  state_ = State::Running;

  if (has_wall_time())
    start_time_ = now->Now();
  if (has_thread_time())
    start_thread_time_ = base::ThreadTicks::Now();
}

void TaskQueue::TaskTiming::RecordTaskEnd(LazyNow* now) {
  DCHECK(state_ == State::Running || state_ == State::Finished);
  if (state_ == State::Finished)
    return;
  state_ = State::Finished;

  if (has_wall_time())
    end_time_ = now->Now();
  if (has_thread_time())
    end_thread_time_ = base::ThreadTicks::Now();
}

void TaskQueue::ShutdownTaskQueue() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  // TODO(crbug.com/1413795): Fix that some task queues get shut down more than
  // once.
  if (!impl_) {
    return;
  }
  if (!sequence_manager_) {
    TakeTaskQueueImpl().reset();
    return;
  }
  sequence_manager_->UnregisterTaskQueueImpl(TakeTaskQueueImpl());
}

scoped_refptr<SingleThreadTaskRunner> TaskQueue::CreateTaskRunner(
    TaskType task_type) {
  // We only need to lock if we're not on the main thread.
  base::internal::CheckedAutoLockMaybe lock(IsOnMainThread() ? &impl_lock_
                                                             : nullptr);
  DCHECK(impl_);
  return impl_->CreateTaskRunner(task_type);
}

std::unique_ptr<TaskQueue::QueueEnabledVoter>
TaskQueue::CreateQueueEnabledVoter() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  return impl_->CreateQueueEnabledVoter();
}

bool TaskQueue::IsQueueEnabled() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  return impl_->IsQueueEnabled();
}

bool TaskQueue::IsEmpty() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  return impl_->IsEmpty();
}

size_t TaskQueue::GetNumberOfPendingTasks() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  return impl_->GetNumberOfPendingTasks();
}

bool TaskQueue::HasTaskToRunImmediatelyOrReadyDelayedTask() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  return impl_->HasTaskToRunImmediatelyOrReadyDelayedTask();
}

absl::optional<WakeUp> TaskQueue::GetNextDesiredWakeUp() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  return impl_->GetNextDesiredWakeUp();
}

void TaskQueue::UpdateWakeUp(LazyNow* lazy_now) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  impl_->UpdateWakeUp(lazy_now);
}

void TaskQueue::SetQueuePriority(TaskQueue::QueuePriority priority) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  impl_->SetQueuePriority(priority);
}

TaskQueue::QueuePriority TaskQueue::GetQueuePriority() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  // TODO(crbug.com/1413795): change this to DCHECK(impl_) since task queues
  // should not be used after shutdown.
  DCHECK(impl_);
  return impl_->GetQueuePriority();
}

void TaskQueue::AddTaskObserver(TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  impl_->AddTaskObserver(task_observer);
}

void TaskQueue::RemoveTaskObserver(TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  impl_->RemoveTaskObserver(task_observer);
}

void TaskQueue::InsertFence(InsertFencePosition position) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  impl_->InsertFence(position);
}

void TaskQueue::InsertFenceAt(TimeTicks time) {
  impl_->InsertFenceAt(time);
}

void TaskQueue::RemoveFence() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  impl_->RemoveFence();
}

bool TaskQueue::HasActiveFence() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  return impl_->HasActiveFence();
}

bool TaskQueue::BlockedByFence() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  return impl_->BlockedByFence();
}

const char* TaskQueue::GetName() const {
  return perfetto::protos::pbzero::SequenceManagerTask::QueueName_Name(name_);
}

void TaskQueue::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("name", name_);
}

void TaskQueue::SetThrottler(Throttler* throttler) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  // |throttler| is guaranteed to outlive TaskQueue and TaskQueueImpl lifecycle
  // is controlled by |this|.
  impl_->SetThrottler(throttler);
}

void TaskQueue::ResetThrottler() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  impl_->ResetThrottler();
}

void TaskQueue::SetShouldReportPostedTasksWhenDisabled(bool should_report) {
  impl_->SetShouldReportPostedTasksWhenDisabled(should_report);
}

bool TaskQueue::IsOnMainThread() const {
  return associated_thread_->IsBoundToCurrentThread();
}

std::unique_ptr<internal::TaskQueueImpl> TaskQueue::TakeTaskQueueImpl() {
  base::internal::CheckedAutoLock lock(impl_lock_);
  DCHECK(impl_);
  return std::move(impl_);
}

void TaskQueue::SetOnTaskStartedHandler(OnTaskStartedHandler handler) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  impl_->SetOnTaskStartedHandler(std::move(handler));
}

void TaskQueue::SetOnTaskCompletedHandler(OnTaskCompletedHandler handler) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  impl_->SetOnTaskCompletedHandler(std::move(handler));
}

std::unique_ptr<TaskQueue::OnTaskPostedCallbackHandle>
TaskQueue::AddOnTaskPostedHandler(OnTaskPostedHandler handler) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  return impl_->AddOnTaskPostedHandler(std::move(handler));
}

void TaskQueue::SetTaskExecutionTraceLogger(TaskExecutionTraceLogger logger) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(impl_);
  impl_->SetTaskExecutionTraceLogger(std::move(logger));
}

}  // namespace sequence_manager
}  // namespace base
