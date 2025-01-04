// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/atomicops.h"

#include <atomic>

#include "base/memory/aligned_memory.h"

namespace base::subtle {

void RelaxedAtomicWriteMemcpy(base::span<uint8_t> dst,
                              base::span<const uint8_t> src) {
  CHECK_EQ(dst.size(), src.size());
  size_t bytes = dst.size();
  uint8_t* dst_byte_ptr = dst.data();
  const uint8_t* src_byte_ptr = src.data();
  // Make sure that we can at least copy byte by byte with atomics.
  static_assert(std::atomic_ref<uint8_t>::required_alignment == 1);

  // Alignment for uintmax_t atomics that we use in the happy case.
  constexpr size_t kDesiredAlignment =
      std::atomic_ref<uintmax_t>::required_alignment;

  // Copy byte-by-byte until `dst_byte_ptr` is not properly aligned for
  // the happy case.
  while (bytes > 0 && !IsAligned(dst_byte_ptr, kDesiredAlignment)) {
    std::atomic_ref<uint8_t>(*dst_byte_ptr)
        .store(*src_byte_ptr, std::memory_order_relaxed);
    // SAFETY: We check above that `dst_byte_ptr` and `src_byte_ptr` point
    // to spans of sufficient size.
    UNSAFE_BUFFERS(++dst_byte_ptr);
    UNSAFE_BUFFERS(++src_byte_ptr);
    --bytes;
  }

  // Happy case where both `src_byte_ptr` and `dst_byte_ptr` are both properly
  // aligned and the largest possible atomic is used for copying.
  if (IsAligned(src_byte_ptr, kDesiredAlignment)) {
    while (bytes >= sizeof(uintmax_t)) {
      std::atomic_ref<uintmax_t>(*reinterpret_cast<uintmax_t*>(dst_byte_ptr))
          .store(*reinterpret_cast<const uintmax_t*>(src_byte_ptr),
                 std::memory_order_relaxed);
      // SAFETY: We check above that `dst_byte_ptr` and `src_byte_ptr` point
      // to spans of sufficient size.
      UNSAFE_BUFFERS(dst_byte_ptr += sizeof(uintmax_t));
      UNSAFE_BUFFERS(src_byte_ptr += sizeof(uintmax_t));
      bytes -= sizeof(uintmax_t);
    }
  }

  // Copy what's left after the happy-case byte-by-byte.
  while (bytes > 0) {
    std::atomic_ref<uint8_t>(*dst_byte_ptr)
        .store(*src_byte_ptr, std::memory_order_relaxed);
    // SAFETY: We check above that `dst_byte_ptr` and `src_byte_ptr` point
    // to spans of sufficient size.
    UNSAFE_BUFFERS(++dst_byte_ptr);
    UNSAFE_BUFFERS(++src_byte_ptr);
    --bytes;
  }
}

}  // namespace base::subtle
