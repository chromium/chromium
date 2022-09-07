// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "thread_annotations.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class LOCKABLE Lock {
 public:
  void Acquire() EXCLUSIVE_LOCK_FUNCTION() {}
  void Release() UNLOCK_FUNCTION() {}
};

class SCOPED_LOCKABLE AutoLock {
 public:
  AutoLock(Lock& lock) EXCLUSIVE_LOCK_FUNCTION(lock) : lock_(lock) {
    lock.Acquire();
  }
  ~AutoLock() UNLOCK_FUNCTION() { lock_.Release(); }

 private:
  Lock& lock_;
};

class ThreadSafe {
 public:
  void ExplicitIncrement();
  void ImplicitIncrement();

 private:
  Lock lock_;
  int counter_ GUARDED_BY(lock_);
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

TEST(ThreadAnnotationsTest, ExplicitIncrement) {
  ThreadSafe thread_safe;
  thread_safe.ExplicitIncrement();
}
TEST(ThreadAnnotationsTest, ImplicitIncrement) {
  ThreadSafe thread_safe;
  thread_safe.ImplicitIncrement();
}

}  // anonymous namespace
