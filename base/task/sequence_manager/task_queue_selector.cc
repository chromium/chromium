// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue_selector.h"

#include <bit>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/task/task_features.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace sequence_manager {
namespace internal {

TaskQueueSelector::TaskQueueSelector(
    scoped_refptr<const AssociatedThreadId> associated_thread,
    const SequenceManager::Settings& settings)
    : associated_thread_(std::move(associated_thread)),
#if DCHECK_IS_ON()
      random_task_selection_(settings.random_task_selection_seed != 0),
#endif
      non_empty_set_counts_(
          std::vector<int>(settings.priority_settings.priority_count(), 0)),
      delayed_work_queue_sets_("delayed", this, settings),
      immediate_work_queue_sets_("immediate", this, settings) {
}

TaskQueueSelector::~TaskQueueSelector() = default;

void TaskQueueSelector::AddQueue(internal::TaskQueueImpl* queue,
                                 TaskQueue::QueuePriority priority) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(queue->IsQueueEnabled());
  AddQueueImpl(queue, priority);
}

void TaskQueueSelector::RemoveQueue(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (queue->IsQueueEnabled()) {
    RemoveQueueImpl(queue);
  }
}

void TaskQueueSelector::EnableQueue(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(queue->IsQueueEnabled());
  AddQueueImpl(queue, queue->GetQueuePriority());
  if (task_queue_selector_observer_)
    task_queue_selector_observer_->OnTaskQueueEnabled(queue);
}

void TaskQueueSelector::DisableQueue(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(!queue->IsQueueEnabled());
  RemoveQueueImpl(queue);
}

void TaskQueueSelector::SetQueuePriority(internal::TaskQueueImpl* queue,
                                         TaskQueue::QueuePriority priority) {
  DCHECK_LT(priority, priority_count());
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (queue->IsQueueEnabled()) {
    ChangeSetIndex(queue, priority);
  } else {
    // Disabled queue is not in any set so we can't use ChangeSetIndex here
    // and have to assign priority for the queue itself.
    queue->delayed_work_queue()->AssignSetIndex(priority);
    queue->immediate_work_queue()->AssignSetIndex(priority);
  }
  DCHECK_EQ(priority, queue->GetQueuePriority());
}

void TaskQueueSelector::AddQueueImpl(internal::TaskQueueImpl* queue,
                                     TaskQueue::QueuePriority priority) {
#if DCHECK_IS_ON()
  DCHECK(!CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.AddQueue(queue->delayed_work_queue(), priority);
  immediate_work_queue_sets_.AddQueue(queue->immediate_work_queue(), priority);
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
}

void TaskQueueSelector::ChangeSetIndex(internal::TaskQueueImpl* queue,
                                       TaskQueue::QueuePriority priority) {
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.ChangeSetIndex(queue->delayed_work_queue(),
                                          priority);
  immediate_work_queue_sets_.ChangeSetIndex(queue->immediate_work_queue(),
                                            priority);
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
}

void TaskQueueSelector::RemoveQueueImpl(internal::TaskQueueImpl* queue) {
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.RemoveQueue(queue->delayed_work_queue());
  immediate_work_queue_sets_.RemoveQueue(queue->immediate_work_queue());

#if DCHECK_IS_ON()
  DCHECK(!CheckContainsQueueForTest(queue));
#endif
}

void TaskQueueSelector::WorkQueueSetBecameEmpty(size_t set_index) {
  non_empty_set_counts_[set_index]--;
  DCHECK_GE(non_empty_set_counts_[set_index], 0);

  // There are no delayed or immediate tasks for |set_index| so remove from
  // |active_priority_tracker_|.
  if (non_empty_set_counts_[set_index] == 0) {
    active_priority_tracker_.SetActive(
        static_cast<TaskQueue::QueuePriority>(set_index), false);
  }
}

void TaskQueueSelector::WorkQueueSetBecameNonEmpty(size_t set_index) {
  non_empty_set_counts_[set_index]++;
  DCHECK_LE(non_empty_set_counts_[set_index], kMaxNonEmptySetCount);

  // There is now a delayed or an immediate task for |set_index|, so add to
  // |active_priority_tracker_|.
  if (non_empty_set_counts_[set_index] == 1) {
    bool had_active_priority = active_priority_tracker_.HasActivePriority();
    TaskQueue::QueuePriority priority =
        static_cast<TaskQueue::QueuePriority>(set_index);
    active_priority_tracker_.SetActive(priority, true);
    if (!had_active_priority && task_queue_selector_observer_) {
      task_queue_selector_observer_->OnWorkAvailable();
    }
  }
}

void TaskQueueSelector::CollectSkippedOverLowerPriorityTasks(
    const internal::WorkQueue* selected_work_queue,
    std::vector<const Task*>* result) const {
  delayed_work_queue_sets_.CollectSkippedOverLowerPriorityTasks(
      selected_work_queue, result);
  immediate_work_queue_sets_.CollectSkippedOverLowerPriorityTasks(
      selected_work_queue, result);
}

#if DCHECK_IS_ON() || !defined(NDEBUG)
bool TaskQueueSelector::CheckContainsQueueForTest(
    const internal::TaskQueueImpl* queue) const {
  bool contains_delayed_work_queue =
      delayed_work_queue_sets_.ContainsWorkQueueForTest(
          queue->delayed_work_queue());

  bool contains_immediate_work_queue =
      immediate_work_queue_sets_.ContainsWorkQueueForTest(
          queue->immediate_work_queue());

  DCHECK_EQ(contains_delayed_work_queue, contains_immediate_work_queue);
  return contains_delayed_work_queue;
}
#endif

WorkQueue* TaskQueueSelector::SelectWorkQueueToService(
    SelectTaskOption option) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);

  auto highest_priority = GetHighestPendingPriority(option);
  if (!highest_priority.has_value())
    return nullptr;

  // Select the priority from which we will select a task. Usually this is
  // the highest priority for which we have work, unless we are starving a lower
  // priority.
  TaskQueue::QueuePriority priority = highest_priority.value();

  // For selecting an immediate queue only, the highest priority can be used as
  // a starting priority, but it is required to check work at other priorities.
  // For the case where a delayed task is at a higher priority than an immediate
  // task, HighestActivePriority(...) returns the priority of the delayed task
  // but the resulting queue must be the lower one.
  if (option == SelectTaskOption::kSkipDelayedTask) {
    WorkQueue* queue =
#if DCHECK_IS_ON()
        random_task_selection_
            ? ChooseImmediateOnlyWithPriority<SetOperationRandom>(priority)
            :
#endif
            ChooseImmediateOnlyWithPriority<SetOperationOldest>(priority);
    return queue;
  }

  WorkQueue* queue =
#if DCHECK_IS_ON()
      random_task_selection_ ? ChooseWithPriority<SetOperationRandom>(priority)
                             :
#endif
                             ChooseWithPriority<SetOperationOldest>(priority);

  // If we have selected a delayed task while having an immediate task of the
  // same priority, increase the starvation count.
  if (queue->queue_type() == WorkQueue::QueueType::kDelayed &&
      !immediate_work_queue_sets_.IsSetEmpty(priority)) {
    immediate_starvation_count_++;
  } else {
    immediate_starvation_count_ = 0;
  }
  return queue;
}

Value::Dict TaskQueueSelector::AsValue() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  Value::Dict state;
  state.Set("immediate_starvation_count", immediate_starvation_count_);
  return state;
}

void TaskQueueSelector::SetTaskQueueSelectorObserver(Observer* observer) {
  task_queue_selector_observer_ = observer;
}

std::optional<TaskQueue::QueuePriority>
TaskQueueSelector::GetHighestPendingPriority(SelectTaskOption option) const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (!active_priority_tracker_.HasActivePriority())
    return std::nullopt;

  TaskQueue::QueuePriority highest_priority =
      active_priority_tracker_.HighestActivePriority();
  DCHECK_LT(highest_priority, priority_count());
  if (option != SelectTaskOption::kSkipDelayedTask)
    return highest_priority;

  for (; highest_priority != priority_count(); ++highest_priority) {
    if (active_priority_tracker_.IsActive(highest_priority) &&
        !immediate_work_queue_sets_.IsSetEmpty(highest_priority)) {
      return highest_priority;
    }
  }

  return std::nullopt;
}

void TaskQueueSelector::SetImmediateStarvationCountForTest(
    int immediate_starvation_count) {
  immediate_starvation_count_ = immediate_starvation_count;
}

bool TaskQueueSelector::HasTasksWithPriority(
    TaskQueue::QueuePriority priority) const {
  return !delayed_work_queue_sets_.IsSetEmpty(priority) ||
         !immediate_work_queue_sets_.IsSetEmpty(priority);
}

TaskQueueSelector::ActivePriorityTracker::ActivePriorityTracker() = default;

void TaskQueueSelector::ActivePriorityTracker::SetActive(
    TaskQueue::QueuePriority priority,
    bool is_active) {
  DCHECK_LT(priority, SequenceManager::PrioritySettings::kMaxPriorities);
  DCHECK_NE(IsActive(priority), is_active);
  if (is_active) {
    active_priorities_ |= (size_t{1} << static_cast<size_t>(priority));
  } else {
    active_priorities_ &= ~(size_t{1} << static_cast<size_t>(priority));
  }
}

TaskQueue::QueuePriority
TaskQueueSelector::ActivePriorityTracker::HighestActivePriority() const {
  DCHECK_NE(active_priorities_, 0u);
  return static_cast<TaskQueue::QueuePriority>(
      std::countr_zero(active_priorities_));
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
