// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SHARED_METRICS_BUFFER_H_
#define CC_METRICS_SHARED_METRICS_BUFFER_H_

#include "device/base/synchronization/one_writer_seqlock.h"

namespace cc {
// The struct written in shared memory to transport metrics across
// processes. |data| is protected by the sequence-lock |seq_lock|.
// Note: This template copies data between processes. Any class that uses this
// template would need security review.
template <class T>
struct SharedMetricsBuffer {
  device::OneWriterSeqLock seq_lock;
  T data;
  static_assert(std::is_trivially_copyable<T>::value,
                "Metrics shared across processes need to be trivially "
                "copyable, otherwise it is dangerous to copy it.");

  bool Read(T& out) const {
    const uint32_t kMaxRetries = 5;
    uint32_t retries = 0;
    base::subtle::Atomic32 version;
    do {
      const uint32_t kMaxReadAttempts = 32;
      version = seq_lock.ReadBegin(kMaxReadAttempts);
      device::OneWriterSeqLock::AtomicReaderMemcpy(&out, &data, sizeof(T));
    } while (seq_lock.ReadRetry(version) && ++retries < kMaxRetries);

    // Consider the number of retries less than kMaxRetries as success.
    return retries < kMaxRetries;
  }

  void Write(const T& in) {
    seq_lock.WriteBegin();
    device::OneWriterSeqLock::AtomicWriterMemcpy(&data, &in, sizeof(T));
    seq_lock.WriteEnd();
  }
};

}  // namespace cc

#endif  // CC_METRICS_SHARED_METRICS_BUFFER_H_
