// Copyright (c) 2021 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RECORD_REPLAY_ORDERED_ATOMIC_H_
#define BASE_RECORD_REPLAY_ORDERED_ATOMIC_H_

#include "base/record_replay.h"

#include <atomic>

// Wrapper for std::atomic that ensures accesses to the atomic value are
// ordered consistently between recording and replaying, for cases where
// atomics are used by threads to signal each other.

namespace recordreplay {

template <typename T>
class OrderedAtomic {
 public:
  OrderedAtomic() {
    ordered_lock_id_ = CreateOrderedLock("atomic");
  }

  OrderedAtomic(T initial) : value_(initial) {
    ordered_lock_id_ = CreateOrderedLock("atomic");
  }

  T load(std::memory_order memory_order = std::memory_order_seq_cst) const {
    AutoOrderedLock ordered(ordered_lock_id_);
    return value_.load(memory_order);
  }

  T fetch_add(T v, std::memory_order memory_order = std::memory_order_seq_cst) {
    AutoOrderedLock ordered(ordered_lock_id_);
    return value_.fetch_add(v, memory_order);
  }

  T fetch_sub(T v, std::memory_order memory_order = std::memory_order_seq_cst) {
    AutoOrderedLock ordered(ordered_lock_id_);
    return value_.fetch_sub(v, memory_order);
  }

 private:
  int ordered_lock_id_;
  std::atomic<T> value_;
};

} // namespace recordreplay

#endif // BASE_RECORD_REPLAY_ORDERED_ATOMIC_H_
