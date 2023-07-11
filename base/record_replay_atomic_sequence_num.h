// Copyright (c) 2023 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RECORD_REPLAY_ATOMIC_SEQUENCE_NUM_H_
#define BASE_RECORD_REPLAY_ATOMIC_SEQUENCE_NUM_H_

#include <atomic>

#include "base/record_replay.h"

namespace recordreplay {

// This is a deterministic version of atomic_sequence_num.h.
class AtomicSequenceNumber {
 public:
  constexpr AtomicSequenceNumber() = default;
  AtomicSequenceNumber(const AtomicSequenceNumber&) = delete;
  AtomicSequenceNumber& operator=(const AtomicSequenceNumber&) = delete;

  // Returns an increasing sequence number starts from 0 for each call.
  // This function can be called from any thread without data race.
  inline int GetNext() {
    return (int)recordreplay::RecordReplayValue(
        "AtomicSequenceNumber::GetNext",
        (uintptr_t)seq_.fetch_add(1, std::memory_order_relaxed));
  }

 private:
  std::atomic_int seq_{0};
};

}  // namespace base

#endif  // BASE_RECORD_REPLAY_ATOMIC_SEQUENCE_NUM_H_
