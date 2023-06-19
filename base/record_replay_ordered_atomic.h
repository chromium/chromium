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
    ordered_lock_id_ = AreEventsDisallowed() ? 0 : CreateOrderedLock("atomic");
  }

  OrderedAtomic(T initial) : value_(initial) {
    ordered_lock_id_ = AreEventsDisallowed() ? 0 : CreateOrderedLock("atomic");
  }

  T load(std::memory_order memory_order = std::memory_order_seq_cst) const {
    AutoOrderedLock ordered(ordered_lock_id_);
    return value_.load(memory_order);
  }

  T load_unordered(std::memory_order memory_order = std::memory_order_seq_cst) const {
    if (recordreplay::AreEventsDisallowed()) {
      recordreplay::AutoPassThroughEvents pass_through;
      return value_.load(memory_order);
    }
    return value_.load(memory_order);
  }

  void store(T v, std::memory_order memory_order = std::memory_order_seq_cst) {
    AutoOrderedLock ordered(ordered_lock_id_);
    value_.store(v, memory_order);
  }

  T fetch_add(T v, std::memory_order memory_order = std::memory_order_seq_cst) {
    AutoOrderedLock ordered(ordered_lock_id_);
    return value_.fetch_add(v, memory_order);
  }

  T fetch_sub(T v, std::memory_order memory_order = std::memory_order_seq_cst) {
    AutoOrderedLock ordered(ordered_lock_id_);
    return value_.fetch_sub(v, memory_order);
  }

  T fetch_or(T v, std::memory_order memory_order = std::memory_order_seq_cst) {
    AutoOrderedLock ordered(ordered_lock_id_);
    return value_.fetch_or(v, memory_order);
  }

  T fetch_and(T v, std::memory_order memory_order = std::memory_order_seq_cst) {
    AutoOrderedLock ordered(ordered_lock_id_);
    return value_.fetch_and(v, memory_order);
  }

  T exchange(T v, std::memory_order memory_order = std::memory_order_seq_cst) {
    AutoOrderedLock ordered(ordered_lock_id_);
    return value_.exchange(v, memory_order);
  }

  bool compare_exchange_weak(T& a, T b,
                             std::memory_order memory_order = std::memory_order_seq_cst) {
    AutoOrderedLock ordered(ordered_lock_id_);
    return value_.compare_exchange_weak(a, b, memory_order);
  }

  bool compare_exchange_strong(T& a, T b,
                               std::memory_order memory_order = std::memory_order_seq_cst) {
    AutoOrderedLock ordered(ordered_lock_id_);
    return value_.compare_exchange_strong(a, b, memory_order);
  }

  bool is_lock_free() const {
    return value_.is_lock_free();
  }

  T operator=(T v) {
    store(v);
    return v;
  }

  T operator()() const {
    return load();
  }

  operator bool() const {
    return !!load();
  }

 private:
  int ordered_lock_id_;
  std::atomic<T> value_;
};

} // namespace recordreplay

#endif // BASE_RECORD_REPLAY_ORDERED_ATOMIC_H_
