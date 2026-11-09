// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_checker_impl.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"

namespace base::sequence_manager {

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

TaskQueue::TaskTiming::TaskTiming(bool has_wall_time, bool has_thread_time)
    : has_wall_time_(has_wall_time), has_thread_time_(has_thread_time) {}

void TaskQueue::TaskTiming::RecordTaskStart(LazyNow* now) {
  DCHECK_EQ(State::NotStarted, state_);
  state_ = State::Running;

  if (has_wall_time()) {
    start_time_ = now->Now();
  }
  if (has_thread_time()) {
    start_thread_time_ = base::ThreadTicks::Now();
  }
}

void TaskQueue::TaskTiming::RecordTaskEnd(LazyNow* now) {
  DCHECK(state_ == State::Running || state_ == State::Finished);
  if (state_ == State::Finished) {
    return;
  }
  state_ = State::Finished;

  if (has_wall_time()) {
    end_time_ = now->Now();
  }
  if (has_thread_time()) {
    end_thread_time_ = base::ThreadTicks::Now();
  }
}

void TaskQueue::TaskTiming::RecordUmaOnCpuMetrics(
    const std::string_view& prefix) const {
  if (has_wall_time() && has_thread_time()) {
    base::TimeDelta wall_time = wall_duration();
    base::TimeDelta thread_time = thread_duration();
    if (!wall_time.is_zero() && !thread_time.is_zero()) {
      UmaHistogramCustomMicrosecondsTimes(
          base::StrCat({prefix, ".TaskOnCpuDuration"}), thread_time,
          base::Microseconds(1), base::Milliseconds(100), 50);
      UmaHistogramCustomMicrosecondsTimes(
          base::StrCat({prefix, ".TaskOffCpuDuration"}),
          wall_time - thread_time, base::Microseconds(1),
          base::Milliseconds(100), 50);
      UmaHistogramPercentage(
          base::StrCat({prefix, ".TaskOnCpuPercentage"}),
          base::checked_cast<int>((thread_time * 100).IntDiv(wall_time)));
    }
  }
}

TaskQueue::Handle::Handle(std::unique_ptr<internal::TaskQueueImpl> task_queue)
    : task_queue_(std::move(task_queue)),
      sequence_manager_(task_queue_->GetSequenceManagerWeakPtr()) {}

TaskQueue::Handle::Handle() = default;

TaskQueue::Handle::~Handle() {
  reset();
}

TaskQueue* TaskQueue::Handle::get() const {
  return task_queue_.get();
}

TaskQueue* TaskQueue::Handle::operator->() const {
  return task_queue_.get();
}

void TaskQueue::Handle::BindToCurrentThread() {
  CHECK(task_queue_);
  task_queue_->BindToCurrentThread();
  sequence_manager_ = task_queue_->GetSequenceManagerWeakPtr();
}

void TaskQueue::Handle::reset() {
  if (!task_queue_) {
    return;
  }
  std::unique_ptr<internal::TaskQueueImpl> task_queue = std::move(task_queue_);
  // Sequence manager already unregistered the task queue.
  if (task_queue->IsUnregistered()) {
    return;
  }
  // A bound queue's Handle must be reset on the SequenceManager's thread,
  // which is where `sequence_manager_` is read and checked. For unbound
  // queues, `reset()` only null-checks `TaskQueueImpl::sequence_manager_` via
  // `was_bound()` and never touches `sequence_manager_` (binding and resetting
  // the same queue concurrently is a contract violation).
  if (!task_queue->was_bound()) {
    task_queue->UnregisterTaskQueue();
    return;
  }
  CHECK(sequence_manager_);
  sequence_manager_->UnregisterTaskQueueImpl(std::move(task_queue));
}

TaskQueue::Handle::Handle(TaskQueue::Handle&& other) = default;

TaskQueue::Handle& TaskQueue::Handle::operator=(TaskQueue::Handle&&) = default;

TaskQueue::Handle TaskQueue::CreateUnbound(const TaskQueue::Spec& spec) {
  return TaskQueue::Handle(std::make_unique<internal::TaskQueueImpl>(
      /*sequence_manager=*/nullptr, /*wake_up_queue=*/nullptr, spec));
}

}  // namespace base::sequence_manager
