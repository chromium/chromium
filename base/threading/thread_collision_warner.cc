// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_collision_warner.h"

#include <atomic>
#include <ostream>

#include "base/notreached.h"
#include "base/threading/platform_thread.h"

namespace base {

void DCheckAsserter::warn() {
  NOTREACHED() << "Thread Collision";
}

void ThreadCollisionWarner::EnterSelf() {
  // If the active thread is kInvalidThreadId then I'll write the current thread
  // ID if two or more threads arrive here only one will succeed to write on
  // valid_thread_id_ the current thread ID.
  PlatformThreadId current_thread_id = PlatformThread::CurrentId();
  PlatformThreadId expected = kInvalidThreadId;

  bool ok = valid_thread_id_.compare_exchange_strong(
      expected, current_thread_id, std::memory_order_relaxed,
      std::memory_order_relaxed);
  if (!ok && expected != current_thread_id) {
    // gotcha! a thread is trying to use the same class and that is
    // not current thread.
    asserter_->warn();
  }
  counter_.fetch_add(1, std::memory_order_relaxed);
}

void ThreadCollisionWarner::Enter() {
  PlatformThreadId current_thread_id = PlatformThread::CurrentId();
  PlatformThreadId expected = kInvalidThreadId;

  if (!valid_thread_id_.compare_exchange_strong(expected, current_thread_id,
                                                std::memory_order_relaxed,
                                                std::memory_order_relaxed)) {
    // gotcha! another thread is trying to use the same class.
    asserter_->warn();
  }
  counter_.fetch_add(1, std::memory_order_relaxed);
}

void ThreadCollisionWarner::Leave() {
  if (counter_.fetch_sub(1, std::memory_order_relaxed) == 1) {
    valid_thread_id_.store(kInvalidThreadId, std::memory_order_relaxed);
  }
}

}  // namespace base
