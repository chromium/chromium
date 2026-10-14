// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ATOMIC_SEQUENCE_NUM_H_
#define BASE_ATOMIC_SEQUENCE_NUM_H_

#include <atomic>
#include <type_traits>

namespace base {

// AtomicSequenceNumberT is a thread safe increasing sequence number generator.
// Its constructor doesn't emit a static initializer, so it's safe to use as a
// global variable or static member.
template <typename T>
  requires(std::is_integral<T>::value)
class AtomicSequenceNumberT {
 public:
  constexpr AtomicSequenceNumberT() = default;
  AtomicSequenceNumberT(const AtomicSequenceNumberT&) = delete;
  AtomicSequenceNumberT& operator=(const AtomicSequenceNumberT&) = delete;

  // Returns an increasing sequence number starts from 0 for each call.
  // This function can be called from any thread without data race.
  inline T GetNext() { return seq_.fetch_add(1, std::memory_order_relaxed); }

 private:
  std::atomic<T> seq_{0};
};

using AtomicSequenceNumber = AtomicSequenceNumberT<int>;

}  // namespace base

#endif  // BASE_ATOMIC_SEQUENCE_NUM_H_
