// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/worker_thread_set.h"

#include "base/check_op.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool/task_source.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task/thread_pool/worker_thread.h"
#include "base/test/gtest_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::internal {

namespace {

class MockWorkerThreadDelegate : public WorkerThread::Delegate {
 public:
  WorkerThread::ThreadLabel GetThreadLabel() const override {
    return WorkerThread::ThreadLabel::DEDICATED;
  }
  void OnMainEntry(WorkerThread* worker) override {}
  RegisteredTaskSource GetWork(WorkerThread* worker) override {
    return nullptr;
  }
  RegisteredTaskSource SwapProcessedTask(RegisteredTaskSource task_source,
                                         WorkerThread* worker) override {
    ADD_FAILURE() << "Unexpected call to SwapProcessedTask()";
    return nullptr;
  }
  TimeDelta GetSleepTimeout() override { return TimeDelta::Max(); }
};

class ThreadPoolWorkerSetTest : public testing::Test {
 protected:
  void SetUp() override {
    worker_a_ = MakeRefCounted<WorkerThread>(
        ThreadType::kDefault, std::make_unique<MockWorkerThreadDelegate>(),
        task_tracker_.GetTrackedRef(), 0);
    ASSERT_TRUE(worker_a_);
    worker_b_ = MakeRefCounted<WorkerThread>(
        ThreadType::kDefault, std::make_unique<MockWorkerThreadDelegate>(),
        task_tracker_.GetTrackedRef(), 1);
    ASSERT_TRUE(worker_b_);
    worker_c_ = MakeRefCounted<WorkerThread>(
        ThreadType::kDefault, std::make_unique<MockWorkerThreadDelegate>(),
        task_tracker_.GetTrackedRef(), 2);
    ASSERT_TRUE(worker_c_);
  }

 private:
  TaskTracker task_tracker_;

 protected:
  scoped_refptr<WorkerThread> worker_a_;
  scoped_refptr<WorkerThread> worker_b_;
  scoped_refptr<WorkerThread> worker_c_;
};

}  // namespace

// Verify that Insert() and Take() add/remove values in FIFO order.
TEST_F(ThreadPoolWorkerSetTest, InsertTake) {
  WorkerThreadSet set;
  EXPECT_EQ(nullptr, set.Take());

  EXPECT_TRUE(set.IsEmpty());
  EXPECT_EQ(0U, set.Size());

  set.Insert(worker_a_.get());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(1U, set.Size());

  set.Insert(worker_b_.get());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(2U, set.Size());

  set.Insert(worker_c_.get());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(3U, set.Size());

  WorkerThread* idle_worker = set.Take();
  EXPECT_EQ(idle_worker, worker_a_.get());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(2U, set.Size());

  set.Insert(idle_worker);
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(3U, set.Size());

  EXPECT_EQ(idle_worker, set.Take());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(2U, set.Size());

  EXPECT_TRUE(set.Take());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(1U, set.Size());

  EXPECT_TRUE(set.Take());
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_EQ(0U, set.Size());

  EXPECT_EQ(nullptr, set.Take());
}

// Verify that Peek() returns the correct values in FIFO order.
TEST_F(ThreadPoolWorkerSetTest, PeekPop) {
  WorkerThreadSet set;
  EXPECT_EQ(nullptr, set.Peek());

  EXPECT_TRUE(set.IsEmpty());
  EXPECT_EQ(0U, set.Size());

  set.Insert(worker_a_.get());
  EXPECT_EQ(worker_a_.get(), set.Peek());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(1U, set.Size());

  set.Insert(worker_b_.get());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(2U, set.Size());

  set.Insert(worker_c_.get());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(3U, set.Size());

  WorkerThread* idle_worker = set.Take();
  EXPECT_EQ(worker_a_.get(), idle_worker);
  EXPECT_EQ(worker_b_.get(), set.Peek());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(2U, set.Size());

  EXPECT_EQ(worker_b_.get(), set.Take());
  EXPECT_EQ(worker_c_.get(), set.Peek());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(1U, set.Size());

  EXPECT_EQ(worker_c_.get(), set.Take());
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_EQ(0U, set.Size());

  EXPECT_EQ(nullptr, set.Peek());
}

// Verify that Contains() returns true for workers on the set.
TEST_F(ThreadPoolWorkerSetTest, Contains) {
  WorkerThreadSet set;
  EXPECT_FALSE(set.Contains(worker_a_.get()));
  EXPECT_FALSE(set.Contains(worker_b_.get()));
  EXPECT_FALSE(set.Contains(worker_c_.get()));

  set.Insert(worker_a_.get());
  EXPECT_TRUE(set.Contains(worker_a_.get()));
  EXPECT_FALSE(set.Contains(worker_b_.get()));
  EXPECT_FALSE(set.Contains(worker_c_.get()));

  set.Insert(worker_b_.get());
  EXPECT_TRUE(set.Contains(worker_a_.get()));
  EXPECT_TRUE(set.Contains(worker_b_.get()));
  EXPECT_FALSE(set.Contains(worker_c_.get()));

  set.Insert(worker_c_.get());
  EXPECT_TRUE(set.Contains(worker_a_.get()));
  EXPECT_TRUE(set.Contains(worker_b_.get()));
  EXPECT_TRUE(set.Contains(worker_c_.get()));

  WorkerThread* idle_worker = set.Take();
  EXPECT_EQ(idle_worker, worker_a_.get());
  EXPECT_FALSE(set.Contains(worker_a_.get()));
  EXPECT_TRUE(set.Contains(worker_b_.get()));
  EXPECT_TRUE(set.Contains(worker_c_.get()));

  set.Take();

  set.Take();
  EXPECT_FALSE(set.Contains(worker_a_.get()));
  EXPECT_FALSE(set.Contains(worker_b_.get()));
  EXPECT_FALSE(set.Contains(worker_c_.get()));
}

// Verify that a value can be removed by Remove().
TEST_F(ThreadPoolWorkerSetTest, Remove) {
  WorkerThreadSet set;
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_EQ(0U, set.Size());

  set.Insert(worker_a_.get());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(1U, set.Size());

  set.Insert(worker_b_.get());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(2U, set.Size());

  set.Insert(worker_c_.get());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(3U, set.Size());

  set.Remove(worker_b_.get());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(2U, set.Size());

  EXPECT_EQ(worker_a_.get(), set.Take());
  EXPECT_FALSE(set.IsEmpty());
  EXPECT_EQ(1U, set.Size());

  EXPECT_EQ(worker_c_.get(), set.Take());
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_EQ(0U, set.Size());
}

// Verify that a value can be pushed again after it has been removed.
TEST_F(ThreadPoolWorkerSetTest, PushAfterRemove) {
  WorkerThreadSet set;
  EXPECT_EQ(0U, set.Size());

  set.Insert(worker_a_.get());
  EXPECT_EQ(1U, set.Size());

  // Need to also push worker B for this test as it's illegal to Remove() the
  // front of the set.
  set.Insert(worker_b_.get());
  EXPECT_EQ(2U, set.Size());

  set.Remove(worker_b_.get());
  worker_b_->EndUnusedPeriod();
  EXPECT_EQ(1U, set.Size());

  set.Insert(worker_b_.get());
  EXPECT_EQ(2U, set.Size());
}

// Verify that Insert() DCHECKs when a value is inserted twice.
TEST_F(ThreadPoolWorkerSetTest, PushTwice) {
  WorkerThreadSet set;
  set.Insert(worker_a_.get());
  EXPECT_DCHECK_DEATH({ set.Insert(worker_a_.get()); });
}

}  // namespace base::internal
