// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequenced_rust_ffi.h"

#include <atomic>

#include "base/check_op.h"

namespace base {

uint32_t GetThreadIndexForRust() {
  // Indices are never reused, so that a thread that starts after another one
  // exits can't take over a lock the old thread held.
  static constinit std::atomic<uint32_t> next_index{1};
  constinit thread_local uint32_t index = 0;
  if (index == 0) {
    index = next_index.fetch_add(1, std::memory_order_relaxed);
    // The counter wrapped around. Crash rather than reuse an index.
    CHECK_NE(index, 0u);
  }
  return index;
}

}  // namespace base
