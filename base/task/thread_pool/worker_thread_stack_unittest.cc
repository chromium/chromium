// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/worker_thread_stack.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool/task_source.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task/thread_pool/worker_thread.h"
#include "base/test/gtest_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

class MockWorkerThreadDelegate : public WorkerThread::Delegate {
 public:
  WorkerThread::ThreadLabel GetThreadLabel() const override {
    return WorkerThread::ThreadLabel::DEDICATED;
  }
  void OnMainEntry(const WorkerThread* worker) override {}
  RegisteredTaskSource GetWork(WorkerThread* worker) override {
    return nullptr;
  }
  void DidProcessTask(RegisteredTaskSource task_source) override {
    ADD_FAILURE() << "Unexpected call to DidRunTask()";
  }
  TimeDelta GetSleepTimeout() override { return TimeDelta::Max(); }
};

class ThreadPoolWorkerStackTest : public testing::Test {
 protected:
  void SetUp() override {
    worker_a_ = MakeRefCounted<WorkerThread>(
        ThreadPriority::NORMAL, std::make_unique<MockWorkerThreadDelegate>(),
        task_tracker_.GetTrackedRef());
    ASSERT_TRUE(worker_a_);
    worker_b_ = MakeRefCounted<WorkerThread>(
        ThreadPriority::NORMAL, std::make_unique<MockWorkerThreadDelegate>(),
        task_tracker_.GetTrackedRef());
    ASSERT_TRUE(worker_b_);
    worker_c_ = MakeRefCounted<WorkerThread>(
        ThreadPriority::NORMAL, std::make_unique<MockWorkerThreadDelegate>(),
        task_tracker_.GetTrackedRef());
    ASSERT_TRUE(worker_c_);
  }

 private:
  TaskTracker task_tracker_{"Test"};

 protected:
  scoped_refptr<WorkerThread> worker_a_;
  scoped_refptr<WorkerThread> worker_b_;
  scoped_refptr<WorkerThread> worker_c_;
};

}  // namespace

// Verify that Push() and Pop() add/remove values in FIFO order.
TEST_F(ThreadPoolWorkerStackTest, PushPop) {
  WorkerThreadStack stack;
  EXPECT_EQ(nullptr, stack.Pop());

  EXPECT_TRUE(stack.IsEmpty());
  EXPECT_EQ(0U, stack.Size());

  stack.Push(worker_a_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(1U, stack.Size());

  stack.Push(worker_b_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  stack.Push(worker_c_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(3U, stack.Size());

  EXPECT_EQ(worker_c_.get(), stack.Pop());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  stack.Push(worker_c_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(3U, stack.Size());

  EXPECT_EQ(worker_c_.get(), stack.Pop());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  EXPECT_EQ(worker_b_.get(), stack.Pop());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(1U, stack.Size());

  EXPECT_EQ(worker_a_.get(), stack.Pop());
  EXPECT_TRUE(stack.IsEmpty());
  EXPECT_EQ(0U, stack.Size());

  EXPECT_EQ(nullptr, stack.Pop());
}

// Verify that Peek() returns the correct values in FIFO order.
TEST_F(ThreadPoolWorkerStackTest, PeekPop) {
  WorkerThreadStack stack;
  EXPECT_EQ(nullptr, stack.Peek());

  EXPECT_TRUE(stack.IsEmpty());
  EXPECT_EQ(0U, stack.Size());

  stack.Push(worker_a_.get());
  EXPECT_EQ(worker_a_.get(), stack.Peek());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(1U, stack.Size());

  stack.Push(worker_b_.get());
  EXPECT_EQ(worker_b_.get(), stack.Peek());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  stack.Push(worker_c_.get());
  EXPECT_EQ(worker_c_.get(), stack.Peek());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(3U, stack.Size());

  EXPECT_EQ(worker_c_.get(), stack.Pop());
  EXPECT_EQ(worker_b_.get(), stack.Peek());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  EXPECT_EQ(worker_b_.get(), stack.Pop());
  EXPECT_EQ(worker_a_.get(), stack.Peek());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(1U, stack.Size());

  EXPECT_EQ(worker_a_.get(), stack.Pop());
  EXPECT_TRUE(stack.IsEmpty());
  EXPECT_EQ(0U, stack.Size());

  EXPECT_EQ(nullptr, stack.Peek());
}

// Verify that Contains() returns true for workers on the stack.
TEST_F(ThreadPoolWorkerStackTest, Contains) {
  WorkerThreadStack stack;
  EXPECT_FALSE(stack.Contains(worker_a_.get()));
  EXPECT_FALSE(stack.Contains(worker_b_.get()));
  EXPECT_FALSE(stack.Contains(worker_c_.get()));

  stack.Push(worker_a_.get());
  EXPECT_TRUE(stack.Contains(worker_a_.get()));
  EXPECT_FALSE(stack.Contains(worker_b_.get()));
  EXPECT_FALSE(stack.Contains(worker_c_.get()));

  stack.Push(worker_b_.get());
  EXPECT_TRUE(stack.Contains(worker_a_.get()));
  EXPECT_TRUE(stack.Contains(worker_b_.get()));
  EXPECT_FALSE(stack.Contains(worker_c_.get()));

  stack.Push(worker_c_.get());
  EXPECT_TRUE(stack.Contains(worker_a_.get()));
  EXPECT_TRUE(stack.Contains(worker_b_.get()));
  EXPECT_TRUE(stack.Contains(worker_c_.get()));

  stack.Pop();
  EXPECT_TRUE(stack.Contains(worker_a_.get()));
  EXPECT_TRUE(stack.Contains(worker_b_.get()));
  EXPECT_FALSE(stack.Contains(worker_c_.get()));

  stack.Pop();
  EXPECT_TRUE(stack.Contains(worker_a_.get()));
  EXPECT_FALSE(stack.Contains(worker_b_.get()));
  EXPECT_FALSE(stack.Contains(worker_c_.get()));

  stack.Pop();
  EXPECT_FALSE(stack.Contains(worker_a_.get()));
  EXPECT_FALSE(stack.Contains(worker_b_.get()));
  EXPECT_FALSE(stack.Contains(worker_c_.get()));
}

// Verify that a value can be removed by Remove().
TEST_F(ThreadPoolWorkerStackTest, Remove) {
  WorkerThreadStack stack;
  EXPECT_TRUE(stack.IsEmpty());
  EXPECT_EQ(0U, stack.Size());

  stack.Push(worker_a_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(1U, stack.Size());

  stack.Push(worker_b_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  stack.Push(worker_c_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(3U, stack.Size());

  stack.Remove(worker_b_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  EXPECT_EQ(worker_c_.get(), stack.Pop());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(1U, stack.Size());

  EXPECT_EQ(worker_a_.get(), stack.Pop());
  EXPECT_TRUE(stack.IsEmpty());
  EXPECT_EQ(0U, stack.Size());
}

// Verify that a value can be pushed again after it has been removed.
TEST_F(ThreadPoolWorkerStackTest, PushAfterRemove) {
  WorkerThreadStack stack;
  EXPECT_EQ(0U, stack.Size());

  stack.Push(worker_a_.get());
  EXPECT_EQ(1U, stack.Size());

  // Need to also push worker B for this test as it's illegal to Remove() the
  // top of the stack.
  stack.Push(worker_b_.get());
  EXPECT_EQ(2U, stack.Size());

  stack.Remove(worker_a_.get());
  EXPECT_EQ(1U, stack.Size());

  stack.Push(worker_a_.get());
  EXPECT_EQ(2U, stack.Size());
}

// Verify that Push() DCHECKs when a value is inserted twice.
TEST_F(ThreadPoolWorkerStackTest, PushTwice) {
  WorkerThreadStack stack;
  stack.Push(worker_a_.get());
  EXPECT_DCHECK_DEATH({ stack.Push(worker_a_.get()); });
}

}  // namespace internal
}  // namespace base
