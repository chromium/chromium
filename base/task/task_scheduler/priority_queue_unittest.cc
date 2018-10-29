// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/priority_queue.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_scheduler/sequence.h"
#include "base/task/task_scheduler/task.h"
#include "base/task/task_traits.h"
#include "base/test/gtest_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

class ThreadBeginningTransaction : public SimpleThread {
 public:
  explicit ThreadBeginningTransaction(PriorityQueue* priority_queue)
      : SimpleThread("ThreadBeginningTransaction"),
        priority_queue_(priority_queue) {}

  // SimpleThread:
  void Run() override {
    std::unique_ptr<PriorityQueue::Transaction> transaction =
        priority_queue_->BeginTransaction();
    transaction_began_.Signal();
  }

  void ExpectTransactionDoesNotBegin() {
    // After a few milliseconds, the call to BeginTransaction() should not have
    // returned.
    EXPECT_FALSE(
        transaction_began_.TimedWait(TimeDelta::FromMilliseconds(250)));
  }

 private:
  PriorityQueue* const priority_queue_;
  WaitableEvent transaction_began_;

  DISALLOW_COPY_AND_ASSIGN(ThreadBeginningTransaction);
};

}  // namespace

TEST(TaskSchedulerPriorityQueueTest, PushPopPeek) {
  // Create test sequences.
  scoped_refptr<Sequence> sequence_a =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::USER_VISIBLE));
  sequence_a->PushTask(Task(FROM_HERE, DoNothing(), TimeDelta()));
  SequenceSortKey sort_key_a = sequence_a->GetSortKey();

  scoped_refptr<Sequence> sequence_b =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::USER_BLOCKING));
  sequence_b->PushTask(Task(FROM_HERE, DoNothing(), TimeDelta()));
  SequenceSortKey sort_key_b = sequence_b->GetSortKey();

  scoped_refptr<Sequence> sequence_c =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::USER_BLOCKING));
  sequence_c->PushTask(Task(FROM_HERE, DoNothing(), TimeDelta()));
  SequenceSortKey sort_key_c = sequence_c->GetSortKey();

  scoped_refptr<Sequence> sequence_d =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::BEST_EFFORT));
  sequence_d->PushTask(Task(FROM_HERE, DoNothing(), TimeDelta()));
  SequenceSortKey sort_key_d = sequence_d->GetSortKey();

  // Create a PriorityQueue and a Transaction.
  PriorityQueue pq;
  auto transaction(pq.BeginTransaction());
  EXPECT_TRUE(transaction->IsEmpty());

  // Push |sequence_a| in the PriorityQueue. It becomes the sequence with the
  // highest priority.
  transaction->Push(sequence_a, sort_key_a);
  EXPECT_EQ(sort_key_a, transaction->PeekSortKey());

  // Push |sequence_b| in the PriorityQueue. It becomes the sequence with the
  // highest priority.
  transaction->Push(sequence_b, sort_key_b);
  EXPECT_EQ(sort_key_b, transaction->PeekSortKey());

  // Push |sequence_c| in the PriorityQueue. |sequence_b| is still the sequence
  // with the highest priority.
  transaction->Push(sequence_c, sort_key_c);
  EXPECT_EQ(sort_key_b, transaction->PeekSortKey());

  // Push |sequence_d| in the PriorityQueue. |sequence_b| is still the sequence
  // with the highest priority.
  transaction->Push(sequence_d, sort_key_d);
  EXPECT_EQ(sort_key_b, transaction->PeekSortKey());

  // Pop |sequence_b| from the PriorityQueue. |sequence_c| becomes the sequence
  // with the highest priority.
  EXPECT_EQ(sequence_b, transaction->PopSequence());
  EXPECT_EQ(sort_key_c, transaction->PeekSortKey());

  // Pop |sequence_c| from the PriorityQueue. |sequence_a| becomes the sequence
  // with the highest priority.
  EXPECT_EQ(sequence_c, transaction->PopSequence());
  EXPECT_EQ(sort_key_a, transaction->PeekSortKey());

  // Pop |sequence_a| from the PriorityQueue. |sequence_d| becomes the sequence
  // with the highest priority.
  EXPECT_EQ(sequence_a, transaction->PopSequence());
  EXPECT_EQ(sort_key_d, transaction->PeekSortKey());

  // Pop |sequence_d| from the PriorityQueue. It is now empty.
  EXPECT_EQ(sequence_d, transaction->PopSequence());
  EXPECT_TRUE(transaction->IsEmpty());
}

// Check that creating Transactions on the same thread for 2 unrelated
// PriorityQueues causes a crash.
TEST(TaskSchedulerPriorityQueueTest, IllegalTwoTransactionsSameThread) {
  PriorityQueue pq_a;
  PriorityQueue pq_b;

  EXPECT_DCHECK_DEATH({
    std::unique_ptr<PriorityQueue::Transaction> transaction_a =
        pq_a.BeginTransaction();
    std::unique_ptr<PriorityQueue::Transaction> transaction_b =
        pq_b.BeginTransaction();
  });
}

// Check that it is possible to begin multiple Transactions for the same
// PriorityQueue on different threads. The call to BeginTransaction() on the
// second thread should block until the Transaction has ended on the first
// thread.
TEST(TaskSchedulerPriorityQueueTest, TwoTransactionsTwoThreads) {
  PriorityQueue pq;

  // Call BeginTransaction() on this thread and keep the Transaction alive.
  std::unique_ptr<PriorityQueue::Transaction> transaction =
      pq.BeginTransaction();

  // Call BeginTransaction() on another thread.
  ThreadBeginningTransaction thread_beginning_transaction(&pq);
  thread_beginning_transaction.Start();

  // After a few milliseconds, the call to BeginTransaction() on the other
  // thread should not have returned.
  thread_beginning_transaction.ExpectTransactionDoesNotBegin();

  // End the Transaction on the current thread.
  transaction.reset();

  // The other thread should exit after its call to BeginTransaction() returns.
  thread_beginning_transaction.Join();
}

}  // namespace internal
}  // namespace base
