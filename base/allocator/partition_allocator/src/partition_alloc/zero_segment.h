// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_ZERO_SEGMENT_H_
#define PARTITION_ALLOC_ZERO_SEGMENT_H_

#include <cstddef>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_config.h"

// The feature is not applicable to 32-bit address space.
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)

namespace partition_alloc::internal {

// Manages the user-space zero segment (`[0, 4 GiB + 1 MiB)` by default),
// ensuring that low virtual addresses cannot be allocated as writable memory.
//
// On non-ASan builds this is initialized from `PartitionAddressSpace::Init()`.
// On Linux/ChromeOS/Android ASan builds (`!USE_PARTITION_ALLOC_AS_MALLOC`),
// this is initialized via an early startup constructor to protect `LowMem`
// (`PROT_NONE`) and `LowShadow` (`PROT_READ`) before `main()`.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) ZeroSegment {
 public:
#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT)
  // Reserves the zero segment. Idempotent; safe to call before
  // `PartitionAddressSpace::Init()`.
  //
  // This function must only be called from the main thread.
  static void Init();
#endif

  PA_ALWAYS_INLINE static size_t GetSize() {
#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT)
    return zero_segment_size_;
#else
    return 0;
#endif
  }

#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT) && PA_BUILDFLAG(IS_ASAN) && \
    PA_CONFIG(HAS_LINUX_KERNEL)
  // Re-runs ASan zero-segment initialization without the one-shot guard,
  // simulating a second DSO that statically links PartitionAlloc.
  static size_t ReinitForAsanForTesting();
#endif

  // ZeroSegment is a static-only class.
  ZeroSegment() = delete;
  ZeroSegment(const ZeroSegment&) = delete;
  void* operator new(size_t) = delete;
  void* operator new(size_t, void*) = delete;

 private:
#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT) && PA_BUILDFLAG(IS_ASAN) && \
    PA_CONFIG(HAS_LINUX_KERNEL)
  static size_t InitForAsan(size_t min_zero_segment_size);
#endif

#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT)
  static size_t zero_segment_size_;
#endif
};

}  // namespace partition_alloc::internal

#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

#endif  // PARTITION_ALLOC_ZERO_SEGMENT_H_
