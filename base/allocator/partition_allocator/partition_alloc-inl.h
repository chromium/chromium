// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_INL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_INL_H_

#include <cstring>

#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/random.h"
#include "build/build_config.h"

// Prefetch *x into memory.
#if defined(__clang__) || defined(COMPILER_GCC)
#define PA_PREFETCH(x) __builtin_prefetch(x)
#else
#define PA_PREFETCH(x)
#endif

namespace partition_alloc::internal {

// This is a `memset` that resists being optimized away. Adapted from
// boringssl/src/crypto/mem.c. (Copying and pasting is bad, but //base can't
// depend on //third_party, and this is small enough.)
ALWAYS_INLINE void SecureMemset(void* ptr, uint8_t value, size_t size) {
  memset(ptr, value, size);

  // As best as we can tell, this is sufficient to break any optimisations that
  // might try to eliminate "superfluous" memsets. If there's an easy way to
  // detect memset_s, it would be better to use that.
  __asm__ __volatile__("" : : "r"(ptr) : "memory");
}

// Returns true if we've hit the end of a random-length period. We don't want to
// invoke `RandomValue` too often, because we call this function in a hot spot
// (`Free`), and `RandomValue` incurs the cost of atomics.
#if !DCHECK_IS_ON()
ALWAYS_INLINE bool RandomPeriod() {
  static thread_local uint8_t counter = 0;
  if (UNLIKELY(counter == 0)) {
    // It's OK to truncate this value.
    counter = static_cast<uint8_t>(::partition_alloc::internal::RandomValue());
  }
  // If `counter` is 0, this will wrap. That is intentional and OK.
  counter--;
  return counter == 0;
}
#endif  // !DCHECK_IS_ON()

}  // namespace partition_alloc::internal

namespace base::internal {

// TODO(https://crbug.com/1288247): Remove these 'using' declarations once
// the migration to the new namespaces gets done.
using ::partition_alloc::internal::SecureMemset;
#if !DCHECK_IS_ON()
using ::partition_alloc::internal::RandomPeriod;
#endif  // !DCHECK_IS_ON()

}  // namespace base::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_INL_H_
