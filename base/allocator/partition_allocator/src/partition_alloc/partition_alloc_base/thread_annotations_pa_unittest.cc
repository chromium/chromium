// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/thread_annotations.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PA_LOCKABLE Lock {
 public:
  void Acquire() PA_EXCLUSIVE_LOCK_FUNCTION() {}
  void Release() PA_UNLOCK_FUNCTION() {}
};

class PA_SCOPED_LOCKABLE AutoLock {
 public:
  AutoLock(Lock& lock) PA_EXCLUSIVE_LOCK_FUNCTION(lock) : lock_(lock) {
    lock.Acquire();
  }
  ~AutoLock() PA_UNLOCK_FUNCTION() { lock_.Release(); }

 private:
  Lock& lock_;
};

class ThreadSafe {
 public:
  void ExplicitIncrement();
  void ImplicitIncrement();

 private:
  Lock lock_;
  int counter_ PA_GUARDED_BY(lock_);
};

void ThreadSafe::ExplicitIncrement() {
  lock_.Acquire();
  ++counter_;
  lock_.Release();
}

void ThreadSafe::ImplicitIncrement() {
  AutoLock auto_lock(lock_);
  counter_++;
}

TEST(PartitionAllocThreadAnnotationsTest, ExplicitIncrement) {
  ThreadSafe thread_safe;
  thread_safe.ExplicitIncrement();
}
TEST(PartitionAllocThreadAnnotationsTest, ImplicitIncrement) {
  ThreadSafe thread_safe;
  thread_safe.ImplicitIncrement();
}

}  // anonymous namespace
