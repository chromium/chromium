// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_PRIORITY_QUEUE_H_
#define BASE_TASK_TASK_SCHEDULER_PRIORITY_QUEUE_H_

#include <memory>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/common/intrusive_heap.h"
#include "base/task/task_scheduler/scheduler_lock.h"
#include "base/task/task_scheduler/sequence.h"
#include "base/task/task_scheduler/sequence_sort_key.h"

namespace base {
namespace internal {

// A PriorityQueue holds Sequences of Tasks. This class is thread-safe.
class BASE_EXPORT PriorityQueue {
 public:
  // A Transaction can perform multiple operations atomically on a
  // PriorityQueue. While a Transaction is alive, it is guaranteed that nothing
  // else will access the PriorityQueue.
  //
  // A Worker needs to be able to Peek sequences from both its PriorityQueues
  // (single-threaded and shared) and then Pop the sequence with the highest
  // priority. If the Peek and the Pop are done through the same Transaction, it
  // is guaranteed that the PriorityQueue hasn't changed between the 2
  // operations.
  class BASE_EXPORT Transaction {
   public:
    ~Transaction();

    // Inserts |sequence| in the PriorityQueue with |sequence_sort_key|.
    // Note: |sequence_sort_key| is required as a parameter instead of being
    // extracted from |sequence| in Push() to avoid this Transaction having a
    // lock interdependency with |sequence|.
    void Push(scoped_refptr<Sequence> sequence,
              const SequenceSortKey& sequence_sort_key);

    // Returns a reference to the SequenceSortKey representing the priority of
    // the highest pending task in this PriorityQueue. The reference becomes
    // invalid the next time that this PriorityQueue is modified.
    // Cannot be called on an empty PriorityQueue.
    const SequenceSortKey& PeekSortKey() const;

    // Removes and returns the highest priority Sequence in this PriorityQueue.
    // Cannot be called on an empty PriorityQueue.
    scoped_refptr<Sequence> PopSequence();

    // Returns true if the PriorityQueue is empty.
    bool IsEmpty() const;

    // Returns the number of Sequences in the PriorityQueue.
    size_t Size() const;

   private:
    friend class PriorityQueue;

    explicit Transaction(PriorityQueue* outer_queue);

    // Holds the lock of |outer_queue_| for the lifetime of this Transaction.
    AutoSchedulerLock auto_lock_;

    PriorityQueue* const outer_queue_;

    DISALLOW_COPY_AND_ASSIGN(Transaction);
  };

  PriorityQueue();

  ~PriorityQueue();

  // Begins a Transaction. This method cannot be called on a thread which has an
  // active Transaction unless the last Transaction created on the thread was
  // for the allowed predecessor specified in the constructor of this
  // PriorityQueue.
  std::unique_ptr<Transaction> BeginTransaction();

  const SchedulerLock* container_lock() const { return &container_lock_; }

 private:
  // A class combining a Sequence and the SequenceSortKey that determines its
  // position in a PriorityQueue.
  class SequenceAndSortKey;

  using ContainerType = IntrusiveHeap<SequenceAndSortKey>;

  // Synchronizes access to |container_|.
  SchedulerLock container_lock_;

  ContainerType container_;

  DISALLOW_COPY_AND_ASSIGN(PriorityQueue);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_PRIORITY_QUEUE_H_
