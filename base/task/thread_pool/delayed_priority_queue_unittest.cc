// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/delayed_priority_queue.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/sequence.h"
#include "base/task/thread_pool/task.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::internal {

namespace {

class DelayedPriorityQueueWithSequencesTest : public testing::Test {
 protected:
  scoped_refptr<Sequence> MakeSequenceWithDelayedTask(
      TimeDelta delayed_run_time) {
    // FastForward time to ensure that queue order between task sources is well
    // defined.
    task_environment.FastForwardBy(Microseconds(1));
    scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
        TaskTraits(), nullptr, TaskSourceExecutionMode::kParallel);
    sequence->BeginTransaction().PushDelayedTask(
        Task(FROM_HERE, DoNothing(), TimeTicks::Now(), delayed_run_time));
    return sequence;
  }

  void Push(scoped_refptr<Sequence> task_source) {
    auto delayed_sort_key = task_source->GetDelayedSortKey();
    pq.Push(std::move(task_source), delayed_sort_key);
  }

  test::TaskEnvironment task_environment{
      test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<Sequence> sequence_a =
      MakeSequenceWithDelayedTask(Milliseconds(90));
  TimeTicks sort_key_a = sequence_a->GetDelayedSortKey();

  scoped_refptr<Sequence> sequence_b =
      MakeSequenceWithDelayedTask(Milliseconds(70));
  TimeTicks sort_key_b = sequence_b->GetDelayedSortKey();

  scoped_refptr<Sequence> sequence_c =
      MakeSequenceWithDelayedTask(Milliseconds(80));
  TimeTicks sort_key_c = sequence_c->GetDelayedSortKey();

  scoped_refptr<Sequence> sequence_d =
      MakeSequenceWithDelayedTask(Milliseconds(100));
  TimeTicks sort_key_d = sequence_d->GetDelayedSortKey();

  DelayedPriorityQueue pq;
};

}  // namespace

TEST_F(DelayedPriorityQueueWithSequencesTest, PushPopPeek) {
  EXPECT_TRUE(pq.IsEmpty());

  // Push |sequence_a| in the DelayedPriorityQueue. It becomes the sequence with
  // the earliest delayed runtime.
  Push(sequence_a);
  EXPECT_EQ(sort_key_a, pq.PeekDelayedSortKey());

  // Push |sequence_b| in the DelayedPriorityQueue. It becomes the sequence with
  // the earliest delayed runtime.
  Push(sequence_b);
  EXPECT_EQ(sort_key_b, pq.PeekDelayedSortKey());

  // Push |sequence_c| in the DelayedPriorityQueue. |sequence_b| is still the
  // sequence with the earliest delayed runtime.
  Push(sequence_c);
  EXPECT_EQ(sort_key_b, pq.PeekDelayedSortKey());

  // Push |sequence_d| in the DelayedPriorityQueue. |sequence_b| is still the
  // sequence with the earliest delayed runtime.
  Push(sequence_d);
  EXPECT_EQ(sort_key_b, pq.PeekDelayedSortKey());

  // Pop |sequence_b| from the DelayedPriorityQueue. |sequence_c| becomes the
  // sequence with the earliest delayed runtime.
  EXPECT_EQ(sequence_b, pq.PopTaskSource());
  EXPECT_EQ(sort_key_c, pq.PeekDelayedSortKey());

  // Pop |sequence_c| from the DelayedPriorityQueue. |sequence_a| becomes the
  // sequence with the earliest delayed runtime.
  EXPECT_EQ(sequence_c, pq.PopTaskSource());
  EXPECT_EQ(sort_key_a, pq.PeekDelayedSortKey());

  // Pop |sequence_a| from the DelayedPriorityQueue. |sequence_d| becomes the
  // sequence with the earliest delayed runtime.
  EXPECT_EQ(sequence_a, pq.PopTaskSource());
  EXPECT_EQ(sort_key_d, pq.PeekDelayedSortKey());

  // Pop |sequence_d| from the DelayedPriorityQueue. It is now empty.
  EXPECT_EQ(sequence_d, pq.PopTaskSource());
  EXPECT_TRUE(pq.IsEmpty());
}

TEST_F(DelayedPriorityQueueWithSequencesTest, RemoveSequence) {
  EXPECT_TRUE(pq.IsEmpty());

  // Push all test Sequences into the PriorityQueue. |sequence_b|
  // will be the sequence with the highest priority.
  Push(sequence_a);
  Push(sequence_b);
  Push(sequence_c);
  Push(sequence_d);
  EXPECT_EQ(sort_key_b, pq.PeekDelayedSortKey());

  // Remove |sequence_a| from the PriorityQueue. |sequence_b| is still the
  // sequence with the highest priority.
  EXPECT_TRUE(pq.RemoveTaskSource(sequence_a));
  EXPECT_EQ(sort_key_b, pq.PeekDelayedSortKey());

  // RemoveTaskSource() should return false if called on a sequence not in the
  // PriorityQueue.
  EXPECT_FALSE(pq.RemoveTaskSource(sequence_a));

  // Remove |sequence_b| from the PriorityQueue. |sequence_c| becomes the
  // sequence with the highest priority.
  EXPECT_TRUE(pq.RemoveTaskSource(sequence_b));
  EXPECT_EQ(sort_key_c, pq.PeekDelayedSortKey());

  // Remove |sequence_d| from the PriorityQueue. |sequence_c| is still the
  // sequence with the highest priority.
  EXPECT_TRUE(pq.RemoveTaskSource(sequence_d));
  EXPECT_EQ(sort_key_c, pq.PeekDelayedSortKey());

  // Remove |sequence_c| from the PriorityQueue, making it empty.
  EXPECT_TRUE(pq.RemoveTaskSource(sequence_c));
  EXPECT_TRUE(pq.IsEmpty());

  // Return false if RemoveTaskSource() is called on an empty PriorityQueue.
  EXPECT_FALSE(pq.RemoveTaskSource(sequence_c));
}

// Test that when the top of a task source changes, the delayed queue is
// appropriately rearranged
TEST_F(DelayedPriorityQueueWithSequencesTest, UpdateDelayedSortKey) {
  EXPECT_TRUE(pq.IsEmpty());

  // Push all test Sequences into the PriorityQueue. |sequence_b| becomes the
  // sequence with the highest priority.
  Push(sequence_a);
  Push(sequence_b);
  Push(sequence_c);
  Push(sequence_d);
  EXPECT_EQ(sort_key_b, pq.PeekDelayedSortKey());

  {
    // Push a new delayed task earlier than pq's top into sequence_c.
    // |sequence_c| becomes the sequence on top of the delayed priority queue.
    sequence_c->BeginTransaction().PushDelayedTask(
        Task(FROM_HERE, DoNothing(), TimeTicks::Now(), Milliseconds(60)));

    sort_key_c = sequence_c->GetDelayedSortKey();
    pq.UpdateDelayedSortKey(sequence_c);
    EXPECT_EQ(sort_key_c, pq.PeekDelayedSortKey());
  }

  {
    // Push a new delayed task earlier than pq's top into sequence_d.
    // |sequence_d| becomes the sequence on top of the delayed priority queue.
    sequence_d->BeginTransaction().PushDelayedTask(
        Task(FROM_HERE, DoNothing(), TimeTicks::Now(), Milliseconds(50)));

    sort_key_d = sequence_d->GetDelayedSortKey();
    pq.UpdateDelayedSortKey(sequence_d);
    EXPECT_EQ(sort_key_d, pq.PeekDelayedSortKey());

    // Pop top task source and verify that it is |sequence_d|
    EXPECT_EQ(sequence_d, pq.PopTaskSource());
    // New top should be |sequence_c|
    EXPECT_EQ(sort_key_c, pq.PeekDelayedSortKey());
  }

  {
    EXPECT_EQ(sequence_c, pq.PopTaskSource());
    EXPECT_EQ(sequence_b, pq.PopTaskSource());
    EXPECT_EQ(sequence_a, pq.PopTaskSource());
  }

  {
    // No-op if UpdateSortKey() is called on an empty PriorityQueue.
    pq.UpdateDelayedSortKey(sequence_b);
    EXPECT_TRUE(pq.IsEmpty());
  }
}

}  // namespace base::internal
