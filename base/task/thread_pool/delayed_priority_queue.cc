// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/delayed_priority_queue.h"

#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"

namespace base::internal {

// A class combining a TaskSource and the delayed_sort_key that determines its
// position in a DelayedPriorityQueue. Instances are only mutable via
// take_task_source() which can only be called once and renders its instance
// invalid after the call.
class DelayedPriorityQueue::TaskSourceAndDelayedSortKey {
 public:
  TaskSourceAndDelayedSortKey() = default;
  TaskSourceAndDelayedSortKey(scoped_refptr<TaskSource> task_source,
                              const TimeTicks& delayed_sort_key)
      : task_source_(std::move(task_source)),
        delayed_sort_key_(delayed_sort_key) {
    DCHECK(task_source_);
  }
  TaskSourceAndDelayedSortKey(const TaskSourceAndDelayedSortKey&) = delete;
  TaskSourceAndDelayedSortKey& operator=(const TaskSourceAndDelayedSortKey&) =
      delete;

  // Note: while |task_source_| should always be non-null post-move (i.e. we
  // shouldn't be moving an invalid TaskSourceAndDelayedSortKey around), there
  // can't be a DCHECK(task_source_) on moves as IntrusiveHeap moves elements on
  // pop instead of overwriting them: resulting in the move of a
  // TaskSourceAndDelayedSortKey with a null |task_source_| in
  // Transaction::Pop()'s implementation.
  TaskSourceAndDelayedSortKey(TaskSourceAndDelayedSortKey&& other) = default;
  TaskSourceAndDelayedSortKey& operator=(TaskSourceAndDelayedSortKey&& other) =
      default;

  // Extracts |task_source_| from this object. This object is invalid after this
  // call.
  scoped_refptr<TaskSource> take_task_source() {
    DCHECK(task_source_);
    task_source_->ClearDelayedHeapHandle();
    return std::move(task_source_);
  }

  // Compares this TaskSourceAndDelayedSortKey to |other| based on their
  // respective |delayed_sort_key_|. Used for a max-heap.
  bool operator<(const TaskSourceAndDelayedSortKey& other) const {
    return delayed_sort_key_ < other.delayed_sort_key_;
  }

  // Required by IntrusiveHeap.
  void SetHeapHandle(const HeapHandle& handle) {
    DCHECK(task_source_);
    task_source_->SetDelayedHeapHandle(handle);
  }

  // Required by IntrusiveHeap.
  void ClearHeapHandle() {
    // Ensure |task_source_| is not nullptr, which may be the case if
    // take_task_source() was called before this.
    if (task_source_)
      task_source_->ClearDelayedHeapHandle();
  }

  // Required by IntrusiveHeap.
  HeapHandle GetHeapHandle() const {
    if (task_source_)
      return task_source_->GetDelayedHeapHandle();
    return HeapHandle::Invalid();
  }

  scoped_refptr<TaskSource> task_source() const { return task_source_; }

  TimeTicks delayed_sort_key() const { return delayed_sort_key_; }

 private:
  scoped_refptr<TaskSource> task_source_;
  TimeTicks delayed_sort_key_;
};

DelayedPriorityQueue::DelayedPriorityQueue() = default;

DelayedPriorityQueue::~DelayedPriorityQueue() {
  if (!is_flush_task_sources_on_destroy_enabled_)
    return;

  while (!container_.empty()) {
    auto task_source = PopTaskSource();
    task_source->ClearForTesting();  // IN-TEST
  }
}

DelayedPriorityQueue& DelayedPriorityQueue::operator=(
    DelayedPriorityQueue&& other) = default;

void DelayedPriorityQueue::Push(scoped_refptr<TaskSource> task_source,
                                TimeTicks task_source_delayed_sort_key) {
  container_.insert(TaskSourceAndDelayedSortKey(std::move(task_source),
                                                task_source_delayed_sort_key));
}

const TimeTicks DelayedPriorityQueue::PeekDelayedSortKey() const {
  DCHECK(!IsEmpty());
  return container_.top().delayed_sort_key();
}

scoped_refptr<TaskSource> DelayedPriorityQueue::PeekTaskSource() const {
  DCHECK(!IsEmpty());

  auto& task_source_and_delayed_sort_key = container_.top();
  return task_source_and_delayed_sort_key.task_source();
}

scoped_refptr<TaskSource> DelayedPriorityQueue::PopTaskSource() {
  DCHECK(!IsEmpty());

  auto task_source_and_delayed_sort_key = container_.take_top();
  scoped_refptr<TaskSource> task_source =
      task_source_and_delayed_sort_key.take_task_source();
  return task_source;
}

scoped_refptr<TaskSource> DelayedPriorityQueue::RemoveTaskSource(
    scoped_refptr<TaskSource> task_source) {
  if (IsEmpty())
    return nullptr;

  const HeapHandle heap_handle = task_source->delayed_heap_handle();
  if (!heap_handle.IsValid())
    return nullptr;

  TaskSourceAndDelayedSortKey& task_source_and_delayed_sort_key =
      const_cast<DelayedPriorityQueue::TaskSourceAndDelayedSortKey&>(
          container_.at(heap_handle));
  DCHECK_EQ(task_source_and_delayed_sort_key.task_source(), task_source);
  task_source = task_source_and_delayed_sort_key.take_task_source();

  container_.erase(heap_handle);
  return task_source;
}

void DelayedPriorityQueue::UpdateDelayedSortKey(
    scoped_refptr<TaskSource> task_source) {
  if (IsEmpty())
    return;

  const HeapHandle heap_handle = task_source->delayed_heap_handle();
  if (!heap_handle.IsValid())
    return;

  DCHECK_EQ(container_.at(heap_handle).task_source(), task_source);

  task_source = const_cast<DelayedPriorityQueue::TaskSourceAndDelayedSortKey&>(
                    container_.at(heap_handle))
                    .take_task_source();
  auto delayed_sort_key = task_source->GetDelayedSortKey();
  container_.Replace(
      heap_handle,
      TaskSourceAndDelayedSortKey(std::move(task_source), delayed_sort_key));
}

bool DelayedPriorityQueue::IsEmpty() const {
  return container_.empty();
}

size_t DelayedPriorityQueue::Size() const {
  return container_.size();
}

void DelayedPriorityQueue::EnableFlushTaskSourcesOnDestroyForTesting() {
  DCHECK(!is_flush_task_sources_on_destroy_enabled_);
  is_flush_task_sources_on_destroy_enabled_ = true;
}

// Delayed tasks are ordered by latest_delayed_run_time(). The top task may
// not be the first task eligible to run, but tasks will always become ripe
// before their latest_delayed_run_time().
bool DelayedPriorityQueue::DelayedPriorityQueueComparisonOperator::operator()(
    const TaskSourceAndDelayedSortKey& lhs,
    const TaskSourceAndDelayedSortKey& rhs) const {
  return lhs.delayed_sort_key() > rhs.delayed_sort_key();
}

}  // namespace base::internal
