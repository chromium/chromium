// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/aligned_memory.h"

#include <string.h>

#include <bit>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include <malloc.h>
#endif

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM) && \
    PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/process/memory.h"
#include "partition_alloc/partition_alloc_constants.h"  // nogncheck
#include "partition_alloc/shim/allocator_shim.h"        // nogncheck
#endif

namespace base {

namespace {

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM) && \
    PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// Returns whether `AlignedCalloc()` should get `size` zeroed bytes from the
// allocator shim, rather than from `AlignedAlloc()` and `memset()`. Both ways
// return zeroed memory, so this only affects performance.
bool ShouldUseUncheckedAlignedCalloc(size_t size) {
#if BUILDFLAG(IS_APPLE)
  // Memory from the allocator shim can't be passed to `free()` before
  // PartitionAlloc replaces the default malloc zone. See `UncheckedMalloc()`.
  if (!allocator_shim::IsDefaultAllocatorPartitionRootInitialized()) {
    return false;
  }
#endif
  // PartitionAlloc direct-maps large allocations, so they always get new pages,
  // which are zeroed already: the calloc functions don't zero them again, and
  // the pages that are never used are never faulted in.
  //
  // Smaller allocations usually reuse freed memory, which has to be zeroed
  // anyway, and the calloc functions would zero their whole slot rather than
  // only `size` bytes, so `AlignedAlloc()` and `memset()` are faster for them.
  return partition_alloc::IsAlwaysDirectMapped(size);
}
#endif

}  // namespace

void* AlignedAlloc(size_t size, size_t alignment) {
  DCHECK_GT(size, 0U);
  DCHECK(std::has_single_bit(alignment));
  DCHECK_EQ(alignment % sizeof(void*), 0U);
  void* ptr = nullptr;
#if defined(COMPILER_MSVC)
  ptr = _aligned_malloc(size, alignment);
#elif BUILDFLAG(IS_ANDROID)
  // Android technically supports posix_memalign(), but does not expose it in
  // the current version of the library headers used by Chromium.  Luckily,
  // memalign() on Android returns pointers which can safely be used with
  // free(), so we can use it instead.  Issue filed to document this:
  // http://code.google.com/p/android/issues/detail?id=35391
  ptr = memalign(alignment, size);
#else
  int ret = posix_memalign(&ptr, alignment, size);
  if (ret != 0) {
    DLOG(ERROR) << "posix_memalign() returned with error " << ret;
    ptr = nullptr;
  }
#endif

  // Since aligned allocations may fail for non-memory related reasons, force a
  // crash if we encounter a failed allocation; maintaining consistent behavior
  // with a normal allocation failure in Chrome.
  CHECK(ptr) << "If you crashed here, your aligned allocation is incorrect: "
             << "size=" << size << ", alignment=" << alignment;

  // Sanity check alignment just to be safe.
  DCHECK(IsAligned(ptr, alignment));
  return ptr;
}

void* AlignedCalloc(size_t num_items, size_t size, size_t alignment) {
  const size_t total_size = CheckMul(num_items, size).ValueOrDie();
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM) && \
    PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if (ShouldUseUncheckedAlignedCalloc(total_size)) {
    DCHECK(std::has_single_bit(alignment));
    DCHECK_EQ(alignment % sizeof(void*), 0U);
    // `AlignedFree()` frees this memory like memory from `AlignedAlloc()`: with
    // `_aligned_free()` on Windows, and otherwise with `free()`, which
    // PartitionAlloc lets free memory from its aligned allocation functions.
    void* ptr =
        allocator_shim::UncheckedAlignedCalloc(num_items, size, alignment);
    // `UncheckedAlignedCalloc()` returns null if `total_size` is too large or
    // memory runs out. `AlignedAlloc()` crashes as out of memory in both
    // cases, so do the same.
    if (!ptr) {
      TerminateBecauseOutOfMemory(total_size);
    }
    DCHECK(IsAligned(ptr, alignment));
    return ptr;
  }
#endif
  void* ptr = AlignedAlloc(total_size, alignment);
  // SAFETY: `AlignedAlloc()` returned `total_size` bytes at `ptr`, as it
  // crashes rather than return null. Only a pointer and a size are available
  // here, and `memset()` is much faster than a bounds-checked fill in
  // instrumented builds, where all sizes take this path.
  UNSAFE_BUFFERS(memset(ptr, 0, total_size));
  return ptr;
}

}  // namespace base
