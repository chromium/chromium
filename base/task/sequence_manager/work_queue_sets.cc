// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_queue_sets.h"

#include <optional>

#include "base/check_op.h"
#include "base/task/sequence_manager/task_order.h"
#include "base/task/sequence_manager/work_queue.h"

namespace base {
namespace sequence_manager {
namespace internal {

WorkQueueSets::WorkQueueSets(const char* name,
                             Observer* observer,
                             const SequenceManager::Settings& settings)
    : name_(name),
      work_queue_heaps_(settings.priority_settings.priority_count()),
#if DCHECK_IS_ON()
      last_rand_(settings.random_task_selection_seed),
#endif
      observer_(observer) {
}

WorkQueueSets::~WorkQueueSets() = default;

void WorkQueueSets::AddQueue(WorkQueue* work_queue, size_t set_index) {
  DCHECK(!work_queue->work_queue_sets());
  DCHECK_LT(set_index, work_queue_heaps_.size());
  DCHECK(!work_queue->heap_handle().IsValid());
  std::optional<TaskOrder> key = work_queue->GetFrontTaskOrder();
  work_queue->AssignToWorkQueueSets(this);
  work_queue->AssignSetIndex(set_index);
  if (!key)
    return;
  bool was_empty = work_queue_heaps_[set_index].empty();
  work_queue_heaps_[set_index].insert({*key, work_queue});
  if (was_empty)
    observer_->WorkQueueSetBecameNonEmpty(set_index);
}

void WorkQueueSets::RemoveQueue(WorkQueue* work_queue) {
  DCHECK_EQ(this, work_queue->work_queue_sets());
  work_queue->AssignToWorkQueueSets(nullptr);
  if (!work_queue->heap_handle().IsValid())
    return;
  size_t set_index = work_queue->work_queue_set_index();
  DCHECK_LT(set_index, work_queue_heaps_.size());
  work_queue_heaps_[set_index].erase(work_queue->heap_handle());
  if (work_queue_heaps_[set_index].empty())
    observer_->WorkQueueSetBecameEmpty(set_index);
  DCHECK(!work_queue->heap_handle().IsValid());
}

void WorkQueueSets::ChangeSetIndex(WorkQueue* work_queue, size_t set_index) {
  DCHECK_EQ(this, work_queue->work_queue_sets());
  DCHECK_LT(set_index, work_queue_heaps_.size());
  std::optional<TaskOrder> key = work_queue->GetFrontTaskOrder();
  size_t old_set = work_queue->work_queue_set_index();
  DCHECK_LT(old_set, work_queue_heaps_.size());
  DCHECK_NE(old_set, set_index);
  work_queue->AssignSetIndex(set_index);
  DCHECK_EQ(key.has_value(), work_queue->heap_handle().IsValid());
  if (!key)
    return;
  work_queue_heaps_[old_set].erase(work_queue->heap_handle());
  bool was_empty = work_queue_heaps_[set_index].empty();
  work_queue_heaps_[set_index].insert({*key, work_queue});
  // Invoke `WorkQueueSetBecameNonEmpty()` before `WorkQueueSetBecameEmpty()` so
  // `observer_` doesn't momentarily observe that all work queue sets are empty.
  // TaskQueueSelectorTest.TestDisableEnable will fail if the order changes.
  if (was_empty)
    observer_->WorkQueueSetBecameNonEmpty(set_index);
  if (work_queue_heaps_[old_set].empty()) {
    observer_->WorkQueueSetBecameEmpty(old_set);
  }
}

void WorkQueueSets::OnQueuesFrontTaskChanged(WorkQueue* work_queue) {
  size_t set_index = work_queue->work_queue_set_index();
  DCHECK_EQ(this, work_queue->work_queue_sets());
  DCHECK_LT(set_index, work_queue_heaps_.size());
  DCHECK(work_queue->heap_handle().IsValid());
  DCHECK(!work_queue_heaps_[set_index].empty()) << " set_index = " << set_index;
  if (auto key = work_queue->GetFrontTaskOrder()) {
    // O(log n)
    work_queue_heaps_[set_index].Replace(work_queue->heap_handle(),
                                         {*key, work_queue});
  } else {
    // O(log n)
    work_queue_heaps_[set_index].erase(work_queue->heap_handle());
    DCHECK(!work_queue->heap_handle().IsValid());
    if (work_queue_heaps_[set_index].empty())
      observer_->WorkQueueSetBecameEmpty(set_index);
  }
}

void WorkQueueSets::OnTaskPushedToEmptyQueue(WorkQueue* work_queue) {
  // NOTE if this function changes, we need to keep |WorkQueueSets::AddQueue| in
  // sync.
  DCHECK_EQ(this, work_queue->work_queue_sets());
  std::optional<TaskOrder> key = work_queue->GetFrontTaskOrder();
  DCHECK(key);
  size_t set_index = work_queue->work_queue_set_index();
  DCHECK_LT(set_index, work_queue_heaps_.size())
      << " set_index = " << set_index;
  // |work_queue| should not be in work_queue_heaps_[set_index].
  DCHECK(!work_queue->heap_handle().IsValid());
  bool was_empty = work_queue_heaps_[set_index].empty();
  work_queue_heaps_[set_index].insert({*key, work_queue});
  if (was_empty)
    observer_->WorkQueueSetBecameNonEmpty(set_index);
}

void WorkQueueSets::OnPopMinQueueInSet(WorkQueue* work_queue) {
  // Assume that `work_queue` contains the lowest `TaskOrder`.
  size_t set_index = work_queue->work_queue_set_index();
  DCHECK_EQ(this, work_queue->work_queue_sets());
  DCHECK_LT(set_index, work_queue_heaps_.size());
  DCHECK(!work_queue_heaps_[set_index].empty()) << " set_index = " << set_index;
  DCHECK_EQ(work_queue_heaps_[set_index].top().value, work_queue)
      << " set_index = " << set_index;
  DCHECK(work_queue->heap_handle().IsValid());
  if (auto key = work_queue->GetFrontTaskOrder()) {
    // O(log n)
    work_queue_heaps_[set_index].ReplaceTop({*key, work_queue});
  } else {
    // O(log n)
    work_queue_heaps_[set_index].pop();
    DCHECK(!work_queue->heap_handle().IsValid());
    DCHECK(work_queue_heaps_[set_index].empty() ||
           work_queue_heaps_[set_index].top().value != work_queue);
    if (work_queue_heaps_[set_index].empty()) {
      observer_->WorkQueueSetBecameEmpty(set_index);
    }
  }
}

void WorkQueueSets::OnQueueBlocked(WorkQueue* work_queue) {
  DCHECK_EQ(this, work_queue->work_queue_sets());
  HeapHandle heap_handle = work_queue->heap_handle();
  if (!heap_handle.IsValid())
    return;
  size_t set_index = work_queue->work_queue_set_index();
  DCHECK_LT(set_index, work_queue_heaps_.size());
  work_queue_heaps_[set_index].erase(heap_handle);
  if (work_queue_heaps_[set_index].empty())
    observer_->WorkQueueSetBecameEmpty(set_index);
}

std::optional<WorkQueueAndTaskOrder>
WorkQueueSets::GetOldestQueueAndTaskOrderInSet(size_t set_index) const {
  DCHECK_LT(set_index, work_queue_heaps_.size());
  if (work_queue_heaps_[set_index].empty())
    return std::nullopt;
  const OldestTaskOrder& oldest = work_queue_heaps_[set_index].top();
  DCHECK(oldest.value->heap_handle().IsValid());
#if DCHECK_IS_ON()
  std::optional<TaskOrder> order = oldest.value->GetFrontTaskOrder();
  DCHECK(order && oldest.key == *order);
#endif
  return WorkQueueAndTaskOrder(*oldest.value, oldest.key);
}

#if DCHECK_IS_ON()
std::optional<WorkQueueAndTaskOrder>
WorkQueueSets::GetRandomQueueAndTaskOrderInSet(size_t set_index) const {
  DCHECK_LT(set_index, work_queue_heaps_.size());
  if (work_queue_heaps_[set_index].empty())
    return std::nullopt;
  const OldestTaskOrder& chosen =
      work_queue_heaps_[set_index].begin()[static_cast<long>(
          Random() % work_queue_heaps_[set_index].size())];
#if DCHECK_IS_ON()
  std::optional<TaskOrder> key = chosen.value->GetFrontTaskOrder();
  DCHECK(key && chosen.key == *key);
#endif
  return WorkQueueAndTaskOrder(*chosen.value, chosen.key);
}
#endif

bool WorkQueueSets::IsSetEmpty(size_t set_index) const {
  DCHECK_LT(set_index, work_queue_heaps_.size())
      << " set_index = " << set_index;
  return work_queue_heaps_[set_index].empty();
}

#if DCHECK_IS_ON() || !defined(NDEBUG)
bool WorkQueueSets::ContainsWorkQueueForTest(
    const WorkQueue* work_queue) const {
  std::optional<TaskOrder> task_order = work_queue->GetFrontTaskOrder();

  for (const auto& heap : work_queue_heaps_) {
    for (const OldestTaskOrder& heap_value_pair : heap) {
      if (heap_value_pair.value == work_queue) {
        DCHECK(task_order);
        DCHECK(heap_value_pair.key == *task_order);
        DCHECK_EQ(this, work_queue->work_queue_sets());
        return true;
      }
    }
  }

  if (work_queue->work_queue_sets() == this) {
    DCHECK(!task_order);
    return true;
  }

  return false;
}
#endif

void WorkQueueSets::CollectSkippedOverLowerPriorityTasks(
    const internal::WorkQueue* selected_work_queue,
    std::vector<const Task*>* result) const {
  std::optional<TaskOrder> task_order =
      selected_work_queue->GetFrontTaskOrder();
  CHECK(task_order);
  for (size_t priority = selected_work_queue->work_queue_set_index() + 1;
       priority < work_queue_heaps_.size(); priority++) {
    for (const OldestTaskOrder& pair : work_queue_heaps_[priority]) {
      pair.value->CollectTasksOlderThan(*task_order, result);
    }
  }
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
