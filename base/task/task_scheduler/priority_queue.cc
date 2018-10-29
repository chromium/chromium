// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/priority_queue.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace base {
namespace internal {

// A class combining a Sequence and the SequenceSortKey that determines its
// position in a PriorityQueue. Instances are only mutable via take_sequence()
// which can only be called once and renders its instance invalid after the
// call.
class PriorityQueue::SequenceAndSortKey {
 public:
  SequenceAndSortKey() = default;
  SequenceAndSortKey(scoped_refptr<Sequence> sequence,
                     const SequenceSortKey& sort_key)
      : sequence_(std::move(sequence)), sort_key_(sort_key) {
    DCHECK(sequence_);
  }

  // Note: while |sequence_| should always be non-null post-move (i.e. we
  // shouldn't be moving an invalid SequenceAndSortKey around), there can't be
  // a DCHECK(sequence_) on moves as IntrusiveHeap moves elements on pop
  // instead of overwriting them: resulting in the move of a SequenceAndSortKey
  // with a null |sequence_| in Transaction::Pop()'s implementation.
  SequenceAndSortKey(SequenceAndSortKey&& other) = default;
  SequenceAndSortKey& operator=(SequenceAndSortKey&& other) = default;

  // Extracts |sequence_| from this object. This object is invalid after this
  // call.
  scoped_refptr<Sequence> take_sequence() {
    DCHECK(sequence_);
    return std::move(sequence_);
  }

  // Compares this SequenceAndSortKey to |other| based on their respective
  // |sort_key_|. Required by IntrusiveHeap.
  bool operator<=(const SequenceAndSortKey& other) const {
    return sort_key_ <= other.sort_key_;
  }

  // Required by IntrusiveHeap.
  void SetHeapHandle(const HeapHandle& handle) {}

  // Required by IntrusiveHeap.
  void ClearHeapHandle() {}

  const SequenceSortKey& sort_key() const { return sort_key_; }

 private:
  scoped_refptr<Sequence> sequence_;
  SequenceSortKey sort_key_;

  DISALLOW_COPY_AND_ASSIGN(SequenceAndSortKey);
};

PriorityQueue::Transaction::Transaction(PriorityQueue* outer_queue)
    : auto_lock_(outer_queue->container_lock_), outer_queue_(outer_queue) {}

PriorityQueue::Transaction::~Transaction() = default;

void PriorityQueue::Transaction::Push(
    scoped_refptr<Sequence> sequence,
    const SequenceSortKey& sequence_sort_key) {
  outer_queue_->container_.insert(
      SequenceAndSortKey(std::move(sequence), sequence_sort_key));
}

const SequenceSortKey& PriorityQueue::Transaction::PeekSortKey() const {
  DCHECK(!IsEmpty());
  return outer_queue_->container_.Min().sort_key();
}

scoped_refptr<Sequence> PriorityQueue::Transaction::PopSequence() {
  DCHECK(!IsEmpty());

  // The const_cast on top() is okay since the SequenceAndSortKey is
  // transactionally being popped from |container_| right after and taking its
  // Sequence does not alter its sort order.
  scoped_refptr<Sequence> sequence =
      const_cast<PriorityQueue::SequenceAndSortKey&>(
          outer_queue_->container_.Min())
          .take_sequence();
  outer_queue_->container_.Pop();
  return sequence;
}

bool PriorityQueue::Transaction::IsEmpty() const {
  return outer_queue_->container_.empty();
}

size_t PriorityQueue::Transaction::Size() const {
  return outer_queue_->container_.size();
}

PriorityQueue::PriorityQueue() = default;

PriorityQueue::~PriorityQueue() = default;

std::unique_ptr<PriorityQueue::Transaction> PriorityQueue::BeginTransaction() {
  return WrapUnique(new Transaction(this));
}

}  // namespace internal
}  // namespace base
