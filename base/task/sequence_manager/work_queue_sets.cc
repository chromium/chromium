// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_queue_sets.h"

#include "base/logging.h"

namespace base {
namespace sequence_manager {
namespace internal {

WorkQueueSets::WorkQueueSets(size_t num_sets, const char* name)
    : work_queue_heaps_(num_sets), name_(name) {}

WorkQueueSets::~WorkQueueSets() = default;

void WorkQueueSets::AddQueue(WorkQueue* work_queue, size_t set_index) {
  DCHECK(!work_queue->work_queue_sets());
  DCHECK_LT(set_index, work_queue_heaps_.size());
  EnqueueOrder enqueue_order;
  bool has_enqueue_order = work_queue->GetFrontTaskEnqueueOrder(&enqueue_order);
  work_queue->AssignToWorkQueueSets(this);
  work_queue->AssignSetIndex(set_index);
  if (!has_enqueue_order)
    return;
  work_queue_heaps_[set_index].insert({enqueue_order, work_queue});
}

void WorkQueueSets::RemoveQueue(WorkQueue* work_queue) {
  DCHECK_EQ(this, work_queue->work_queue_sets());
  work_queue->AssignToWorkQueueSets(nullptr);
  base::internal::HeapHandle heap_handle = work_queue->heap_handle();
  if (!heap_handle.IsValid())
    return;
  size_t set_index = work_queue->work_queue_set_index();
  DCHECK_LT(set_index, work_queue_heaps_.size());
  work_queue_heaps_[set_index].erase(heap_handle);
}

void WorkQueueSets::ChangeSetIndex(WorkQueue* work_queue, size_t set_index) {
  DCHECK_EQ(this, work_queue->work_queue_sets());
  DCHECK_LT(set_index, work_queue_heaps_.size());
  EnqueueOrder enqueue_order;
  bool has_enqueue_order = work_queue->GetFrontTaskEnqueueOrder(&enqueue_order);
  size_t old_set = work_queue->work_queue_set_index();
  DCHECK_LT(old_set, work_queue_heaps_.size());
  DCHECK_NE(old_set, set_index);
  work_queue->AssignSetIndex(set_index);
  if (!has_enqueue_order)
    return;
  work_queue_heaps_[old_set].erase(work_queue->heap_handle());
  work_queue_heaps_[set_index].insert({enqueue_order, work_queue});
}

void WorkQueueSets::OnFrontTaskChanged(WorkQueue* work_queue) {
  EnqueueOrder enqueue_order;
  bool has_enqueue_order = work_queue->GetFrontTaskEnqueueOrder(&enqueue_order);
  DCHECK(has_enqueue_order);
  size_t set = work_queue->work_queue_set_index();
  work_queue_heaps_[set].ChangeKey(work_queue->heap_handle(),
                                   {enqueue_order, work_queue});
}

void WorkQueueSets::OnTaskPushedToEmptyQueue(WorkQueue* work_queue) {
  // NOTE if this function changes, we need to keep |WorkQueueSets::AddQueue| in
  // sync.
  DCHECK_EQ(this, work_queue->work_queue_sets());
  EnqueueOrder enqueue_order;
  bool has_enqueue_order = work_queue->GetFrontTaskEnqueueOrder(&enqueue_order);
  DCHECK(has_enqueue_order);
  size_t set_index = work_queue->work_queue_set_index();
  DCHECK_LT(set_index, work_queue_heaps_.size())
      << " set_index = " << set_index;
  // |work_queue| should not be in work_queue_heaps_[set_index].
  DCHECK(!work_queue->heap_handle().IsValid());
  work_queue_heaps_[set_index].insert({enqueue_order, work_queue});
}

void WorkQueueSets::OnPopQueue(WorkQueue* work_queue) {
  // Assume that |work_queue| contains the lowest enqueue_order.
  size_t set_index = work_queue->work_queue_set_index();
  DCHECK_EQ(this, work_queue->work_queue_sets());
  DCHECK_LT(set_index, work_queue_heaps_.size());
  DCHECK(!work_queue_heaps_[set_index].empty()) << " set_index = " << set_index;
  DCHECK_EQ(work_queue_heaps_[set_index].Min().value, work_queue)
      << " set_index = " << set_index;
  DCHECK(work_queue->heap_handle().IsValid());
  EnqueueOrder enqueue_order;
  if (work_queue->GetFrontTaskEnqueueOrder(&enqueue_order)) {
    // O(log n)
    work_queue_heaps_[set_index].ReplaceMin({enqueue_order, work_queue});
  } else {
    // O(log n)
    work_queue_heaps_[set_index].Pop();
    DCHECK(work_queue_heaps_[set_index].empty() ||
           work_queue_heaps_[set_index].Min().value != work_queue);
  }
}

void WorkQueueSets::OnQueueBlocked(WorkQueue* work_queue) {
  DCHECK_EQ(this, work_queue->work_queue_sets());
  base::internal::HeapHandle heap_handle = work_queue->heap_handle();
  if (!heap_handle.IsValid())
    return;
  size_t set_index = work_queue->work_queue_set_index();
  DCHECK_LT(set_index, work_queue_heaps_.size());
  work_queue_heaps_[set_index].erase(heap_handle);
}

bool WorkQueueSets::GetOldestQueueInSet(size_t set_index,
                                        WorkQueue** out_work_queue) const {
  DCHECK_LT(set_index, work_queue_heaps_.size());
  if (work_queue_heaps_[set_index].empty())
    return false;
  *out_work_queue = work_queue_heaps_[set_index].Min().value;
  DCHECK_EQ(set_index, (*out_work_queue)->work_queue_set_index());
  DCHECK((*out_work_queue)->heap_handle().IsValid());
  return true;
}

bool WorkQueueSets::GetOldestQueueAndEnqueueOrderInSet(
    size_t set_index,
    WorkQueue** out_work_queue,
    EnqueueOrder* out_enqueue_order) const {
  DCHECK_LT(set_index, work_queue_heaps_.size());
  if (work_queue_heaps_[set_index].empty())
    return false;
  const OldestTaskEnqueueOrder& oldest = work_queue_heaps_[set_index].Min();
  *out_work_queue = oldest.value;
  *out_enqueue_order = oldest.key;
  EnqueueOrder enqueue_order;
  DCHECK(oldest.value->GetFrontTaskEnqueueOrder(&enqueue_order) &&
         oldest.key == enqueue_order);
  return true;
}

bool WorkQueueSets::IsSetEmpty(size_t set_index) const {
  DCHECK_LT(set_index, work_queue_heaps_.size())
      << " set_index = " << set_index;
  return work_queue_heaps_[set_index].empty();
}

#if DCHECK_IS_ON() || !defined(NDEBUG)
bool WorkQueueSets::ContainsWorkQueueForTest(
    const WorkQueue* work_queue) const {
  EnqueueOrder enqueue_order;
  bool has_enqueue_order = work_queue->GetFrontTaskEnqueueOrder(&enqueue_order);

  for (const base::internal::IntrusiveHeap<OldestTaskEnqueueOrder>& heap :
       work_queue_heaps_) {
    for (const OldestTaskEnqueueOrder& heap_value_pair : heap) {
      if (heap_value_pair.value == work_queue) {
        DCHECK(has_enqueue_order);
        DCHECK_EQ(heap_value_pair.key, enqueue_order);
        DCHECK_EQ(this, work_queue->work_queue_sets());
        return true;
      }
    }
  }

  if (work_queue->work_queue_sets() == this) {
    DCHECK(!has_enqueue_order);
    return true;
  }

  return false;
}
#endif

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
