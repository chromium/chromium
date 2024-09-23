// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_queue.h"

#include <optional>

#include "base/debug/alias.h"
#include "base/task/sequence_manager/fence.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_order.h"
#include "base/task/sequence_manager/work_queue_sets.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace base {
namespace sequence_manager {
namespace internal {

WorkQueue::WorkQueue(TaskQueueImpl* task_queue,
                     const char* name,
                     QueueType queue_type)
    : task_queue_(task_queue), name_(name), queue_type_(queue_type) {}

Value::List WorkQueue::AsValue(TimeTicks now) const {
  Value::List state;
  for (const Task& task : tasks_)
    state.Append(TaskQueueImpl::TaskAsValue(task, now));
  return state;
}

WorkQueue::~WorkQueue() {
  DCHECK(!work_queue_sets_) << task_queue_->GetName() << " : "
                            << work_queue_sets_->GetName() << " : " << name_;
}

const Task* WorkQueue::GetFrontTask() const {
  if (tasks_.empty())
    return nullptr;
  return &tasks_.front();
}

const Task* WorkQueue::GetBackTask() const {
  if (tasks_.empty())
    return nullptr;
  return &tasks_.back();
}

bool WorkQueue::BlockedByFence() const {
  if (!fence_)
    return false;

  // If the queue is empty then any future tasks will have a higher enqueue
  // order and will be blocked. The queue is also blocked if the head is past
  // the fence.
  return tasks_.empty() || tasks_.front().task_order() >= fence_->task_order();
}

std::optional<TaskOrder> WorkQueue::GetFrontTaskOrder() const {
  if (tasks_.empty() || BlockedByFence())
    return std::nullopt;
  // Quick sanity check.
  DCHECK(tasks_.front().task_order() <= tasks_.back().task_order())
      << task_queue_->GetName() << " : " << work_queue_sets_->GetName() << " : "
      << name_;
  return tasks_.front().task_order();
}

void WorkQueue::Push(Task task) {
  bool was_empty = tasks_.empty();
#ifndef NDEBUG
  DCHECK(task.enqueue_order_set());
#endif

  // Make sure the task order is strictly increasing.
  DCHECK(was_empty || tasks_.back().task_order() < task.task_order());
  // Make sure enqueue order is strictly increasing for immediate queues and
  // monotonically increasing for delayed queues.
  DCHECK(was_empty || tasks_.back().enqueue_order() < task.enqueue_order() ||
         (queue_type_ == QueueType::kDelayed &&
          tasks_.back().enqueue_order() == task.enqueue_order()));

  // Amortized O(1).
  tasks_.push_back(std::move(task));

  if (!was_empty)
    return;

  // If we hit the fence, pretend to WorkQueueSets that we're empty.
  if (work_queue_sets_ && !BlockedByFence())
    work_queue_sets_->OnTaskPushedToEmptyQueue(this);
}

WorkQueue::TaskPusher::TaskPusher(WorkQueue* work_queue)
    : work_queue_(work_queue), was_empty_(work_queue->Empty()) {}

WorkQueue::TaskPusher::TaskPusher(TaskPusher&& other)
    : work_queue_(other.work_queue_), was_empty_(other.was_empty_) {
  other.work_queue_ = nullptr;
}

void WorkQueue::TaskPusher::Push(Task task) {
  DCHECK(work_queue_);

#ifndef NDEBUG
  DCHECK(task.enqueue_order_set());
#endif

  // Make sure the task order is strictly increasing.
  DCHECK(work_queue_->tasks_.empty() ||
         work_queue_->tasks_.back().task_order() < task.task_order());
  // Make sure enqueue order is strictly increasing for immediate queues and
  // monotonically increasing for delayed queues.
  DCHECK(work_queue_->tasks_.empty() ||
         work_queue_->tasks_.back().enqueue_order() < task.enqueue_order() ||
         (work_queue_->queue_type_ == QueueType::kDelayed &&
          work_queue_->tasks_.back().enqueue_order() == task.enqueue_order()));

  // Amortized O(1).
  work_queue_->tasks_.push_back(std::move(task));
}

WorkQueue::TaskPusher::~TaskPusher() {
  // If |work_queue_| became non empty and it isn't blocked by a fence then we
  // must notify |work_queue_->work_queue_sets_|.
  if (was_empty_ && work_queue_ && !work_queue_->Empty() &&
      work_queue_->work_queue_sets_ && !work_queue_->BlockedByFence()) {
    work_queue_->work_queue_sets_->OnTaskPushedToEmptyQueue(work_queue_);
  }
}

WorkQueue::TaskPusher WorkQueue::CreateTaskPusher() {
  return TaskPusher(this);
}

void WorkQueue::PushNonNestableTaskToFront(Task task) {
  DCHECK(task.nestable == Nestable::kNonNestable);

  bool was_empty = tasks_.empty();
  bool was_blocked = BlockedByFence();
#ifndef NDEBUG
  DCHECK(task.enqueue_order_set());
#endif

  if (!was_empty) {
    // Make sure the task order is strictly increasing.
    DCHECK(task.task_order() < tasks_.front().task_order())
        << task_queue_->GetName() << " : " << work_queue_sets_->GetName()
        << " : " << name_;
    // Make sure the enqueue order is strictly increasing for immediate queues
    // and monotonically increasing for delayed queues.
    DCHECK(task.enqueue_order() < tasks_.front().enqueue_order() ||
           (queue_type_ == QueueType::kDelayed &&
            task.enqueue_order() == tasks_.front().enqueue_order()))
        << task_queue_->GetName() << " : " << work_queue_sets_->GetName()
        << " : " << name_;
  }

  // Amortized O(1).
  tasks_.push_front(std::move(task));

  if (!work_queue_sets_)
    return;

  // Pretend  to WorkQueueSets that nothing has changed if we're blocked.
  if (BlockedByFence())
    return;

  // Pushing task to front may unblock the fence.
  if (was_empty || was_blocked) {
    work_queue_sets_->OnTaskPushedToEmptyQueue(this);
  } else {
    work_queue_sets_->OnQueuesFrontTaskChanged(this);
  }
}

void WorkQueue::TakeImmediateIncomingQueueTasks() {
  DCHECK(tasks_.empty());

  task_queue_->TakeImmediateIncomingQueueTasks(&tasks_);
  if (tasks_.empty())
    return;

  // If we hit the fence, pretend to WorkQueueSets that we're empty.
  if (work_queue_sets_ && !BlockedByFence())
    work_queue_sets_->OnTaskPushedToEmptyQueue(this);
}

Task WorkQueue::TakeTaskFromWorkQueue() {
  DCHECK(work_queue_sets_);
  DCHECK(!tasks_.empty());

  Task pending_task = std::move(tasks_.front());
  tasks_.pop_front();
  // NB immediate tasks have a different pipeline to delayed ones.
  if (tasks_.empty()) {
    // NB delayed tasks are inserted via Push, no don't need to reload those.
    if (queue_type_ == QueueType::kImmediate) {
      // Short-circuit the queue reload so that OnPopMinQueueInSet does the
      // right thing.
      task_queue_->TakeImmediateIncomingQueueTasks(&tasks_);
    }
    // Since the queue is empty, now is a good time to consider reducing it's
    // capacity if we're wasting memory.
    tasks_.MaybeShrinkQueue();
  }

  DCHECK(work_queue_sets_);
#if DCHECK_IS_ON()
  // If diagnostics are on it's possible task queues are being selected at
  // random so we can't use the (slightly) more efficient OnPopMinQueueInSet.
  work_queue_sets_->OnQueuesFrontTaskChanged(this);
#else
  // OnPopMinQueueInSet calls GetFrontTaskOrder which checks
  // BlockedByFence() so we don't need to here.
  work_queue_sets_->OnPopMinQueueInSet(this);
#endif
  task_queue_->TraceQueueSize();
  return pending_task;
}

bool WorkQueue::RemoveAllCanceledTasksFromFront() {
  if (!work_queue_sets_) {
    return false;
  }

  // Since task destructors could have a side-effect of deleting this task queue
  // we move cancelled tasks into a temporary container which can be emptied
  // without accessing |this|.
  absl::InlinedVector<Task, 8> tasks_to_delete;

  while (!tasks_.empty()) {
    const auto& pending_task = tasks_.front();
    if (pending_task.task && !pending_task.IsCanceled())
      break;
    tasks_to_delete.push_back(std::move(tasks_.front()));
    tasks_.pop_front();
  }
  if (!tasks_to_delete.empty()) {
    if (tasks_.empty()) {
      // NB delayed tasks are inserted via Push, no don't need to reload those.
      if (queue_type_ == QueueType::kImmediate) {
        // Short-circuit the queue reload so that OnPopMinQueueInSet does the
        // right thing.
        task_queue_->TakeImmediateIncomingQueueTasks(&tasks_);
      }
      // Since the queue is empty, now is a good time to consider reducing it's
      // capacity if we're wasting memory.
      tasks_.MaybeShrinkQueue();
    }
    // If we have a valid |heap_handle_| (i.e. we're not blocked by a fence or
    // disabled) then |work_queue_sets_| needs to be told.
    if (heap_handle_.IsValid())
      work_queue_sets_->OnQueuesFrontTaskChanged(this);
    task_queue_->TraceQueueSize();
  }
  return !tasks_to_delete.empty();
}

void WorkQueue::AssignToWorkQueueSets(WorkQueueSets* work_queue_sets) {
  work_queue_sets_ = work_queue_sets;
}

void WorkQueue::AssignSetIndex(size_t work_queue_set_index) {
  work_queue_set_index_ = work_queue_set_index;
}

bool WorkQueue::InsertFenceImpl(Fence fence) {
  DCHECK(!fence_ || fence.task_order() >= fence_->task_order() ||
         fence.IsBlockingFence());
  bool was_blocked_by_fence = BlockedByFence();
  fence_ = fence;
  return was_blocked_by_fence;
}

void WorkQueue::InsertFenceSilently(Fence fence) {
  // Ensure that there is no fence present or a new one blocks queue completely.
  DCHECK(!fence_ || fence_->IsBlockingFence());
  InsertFenceImpl(fence);
}

bool WorkQueue::InsertFence(Fence fence) {
  bool was_blocked_by_fence = InsertFenceImpl(fence);
  if (!work_queue_sets_)
    return false;

  // Moving the fence forward may unblock some tasks.
  if (!tasks_.empty() && was_blocked_by_fence && !BlockedByFence()) {
    work_queue_sets_->OnTaskPushedToEmptyQueue(this);
    return true;
  }
  // Fence insertion may have blocked all tasks in this work queue.
  if (BlockedByFence())
    work_queue_sets_->OnQueueBlocked(this);
  return false;
}

bool WorkQueue::RemoveFence() {
  bool was_blocked_by_fence = BlockedByFence();
  fence_ = std::nullopt;
  if (work_queue_sets_ && !tasks_.empty() && was_blocked_by_fence) {
    work_queue_sets_->OnTaskPushedToEmptyQueue(this);
    return true;
  }
  return false;
}

void WorkQueue::MaybeShrinkQueue() {
  tasks_.MaybeShrinkQueue();
}

void WorkQueue::PopTaskForTesting() {
  if (tasks_.empty())
    return;
  tasks_.pop_front();
}

void WorkQueue::CollectTasksOlderThan(TaskOrder reference,
                                      std::vector<const Task*>* result) const {
  for (const Task& task : tasks_) {
    if (task.task_order() >= reference)
      break;

    result->push_back(&task);
  }
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
