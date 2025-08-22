// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SHARED_METRICS_BUFFER_H_
#define CC_METRICS_SHARED_METRICS_BUFFER_H_

#include <atomic>

#include "device/base/synchronization/one_writer_seqlock.h"

namespace cc {
// The struct written in shared memory to transport metrics across
// processes. |data| is protected by the sequence-lock |seq_lock|.
// Note: This template copies data between processes. Any class that uses this
// template would need security review.
template <class T>
// Non-trivially-copyable `T`s are dangerous to copy across processes.
  requires std::is_trivially_copyable_v<T>
struct SharedMetricsBuffer {
  T data;  // Make sure this is first, so it will be page-aligned when this
           // object is placed in shared memory. This ensures libc++'s alignment
           // requirements for lock-free atomic access will be satisfied even if
           // `alignof(T) < sizeof(T)` (as is true for e.g. `double` on 32-bit
           // x86 Android).
  device::OneWriterSeqLock seq_lock;

  bool Read(T& out) const {
    const uint32_t kMaxRetries = 5;
    uint32_t retries = 0;
    base::subtle::Atomic32 version;
    do {
      const uint32_t kMaxReadAttempts = 32;
      version = seq_lock.ReadBegin(kMaxReadAttempts);
      // TODO(https://github.com/llvm/llvm-project/issues/118378): Remove
      // const_cast.
      out =
          std::atomic_ref(const_cast<T&>(data)).load(std::memory_order_relaxed);
    } while (seq_lock.ReadRetry(version) && ++retries < kMaxRetries);

    // Consider the number of retries less than kMaxRetries as success.
    return retries < kMaxRetries;
  }

  void Write(const T& in) {
    seq_lock.WriteBegin();
    std::atomic_ref(data).store(in, std::memory_order_relaxed);
    seq_lock.WriteEnd();
  }
};

}  // namespace cc

#endif  // CC_METRICS_SHARED_METRICS_BUFFER_H_
