// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/allocator/allocator_impls.h"

#include <cstddef>
#include <cstring>

// //build doesn't depend on //base, so we can't use UNSAFE_BUFFERS macro here.
// We define a local equivalent instead.
#if defined(__clang__)
// clang-format off
#define BUILD_UNSAFE_BUFFERS(...)            \
  _Pragma("clang unsafe_buffer_usage begin") \
  __VA_ARGS__                                \
  _Pragma("clang unsafe_buffer_usage end")
// clang-format on
#else
#define BUILD_UNSAFE_BUFFERS(...) __VA_ARGS__
#endif

#include "build/build_config.h"
#include "build/rust/allocator/buildflags.h"

#if BUILDFLAG(RUST_ALLOCATOR_USES_PARTITION_ALLOC)
#include "partition_alloc/partition_alloc_constants.h"  // nogncheck
#include "partition_alloc/shim/allocator_shim.h"        // nogncheck
#elif BUILDFLAG(RUST_ALLOCATOR_USES_ALIGNED_MALLOC)
#include <cstdlib>
#endif

namespace rust_allocator_internal {

unsigned char* alloc(size_t size, size_t align) {
#if BUILDFLAG(RUST_ALLOCATOR_USES_PARTITION_ALLOC)
  // PartitionAlloc will crash if given an alignment larger than this.
  if (align > partition_alloc::internal::kMaxSupportedAlignment) {
    return nullptr;
  }

  // We use unchecked allocation paths in PartitionAlloc rather than going
  // through its shims in `malloc()` etc so that we can support fallible
  // allocation paths such as Vec::try_reserve without crashing on allocation
  // failure.
  if (align <= alignof(std::max_align_t)) {
    return static_cast<unsigned char*>(allocator_shim::UncheckedAlloc(size));
  } else {
    return static_cast<unsigned char*>(
        allocator_shim::UncheckedAlignedAlloc(size, align));
  }
#elif BUILDFLAG(RUST_ALLOCATOR_USES_ALIGNED_MALLOC)
  return static_cast<unsigned char*>(_aligned_malloc(size, align));
#else
#error This configuration is not supported.
#endif
}

void dealloc(unsigned char* p, size_t size, size_t align) {
#if BUILDFLAG(RUST_ALLOCATOR_USES_PARTITION_ALLOC)
  if (align <= alignof(std::max_align_t)) {
    allocator_shim::UncheckedFree(p);
  } else {
    allocator_shim::UncheckedAlignedFree(p);
  }
#elif BUILDFLAG(RUST_ALLOCATOR_USES_ALIGNED_MALLOC)
  return _aligned_free(p);
#else
#error This configuration is not supported.
#endif
}

unsigned char* realloc(unsigned char* p,
                       size_t old_size,
                       size_t align,
                       size_t new_size) {
#if BUILDFLAG(RUST_ALLOCATOR_USES_PARTITION_ALLOC)
  // We use unchecked allocation paths in PartitionAlloc rather than going
  // through its shims in `malloc()` etc so that we can support fallible
  // allocation paths such as Vec::try_reserve without crashing on allocation
  // failure.
  if (align <= alignof(std::max_align_t)) {
    return static_cast<unsigned char*>(
        allocator_shim::UncheckedRealloc(p, new_size));
  } else {
    return static_cast<unsigned char*>(
        allocator_shim::UncheckedAlignedRealloc(p, new_size, align));
  }
#elif BUILDFLAG(RUST_ALLOCATOR_USES_ALIGNED_MALLOC)
  return static_cast<unsigned char*>(_aligned_realloc(p, new_size, align));
#else
#error This configuration is not supported.
#endif
}

unsigned char* alloc_zeroed(size_t size, size_t align) {
#if BUILDFLAG(RUST_ALLOCATOR_USES_PARTITION_ALLOC) || \
    BUILDFLAG(RUST_ALLOCATOR_USES_ALIGNED_MALLOC)
#if BUILDFLAG(RUST_ALLOCATOR_USES_PARTITION_ALLOC)
  // This check only picks the faster of two ways to get zeroed memory. Both
  // ways return zeroed memory, so a wrong choice only affects performance, not
  // correctness or security.
  //
  // PartitionAlloc direct-maps large allocations, so they always get new pages,
  // which are zeroed already: the calloc functions don't zero them again, and
  // the pages that are never written are never faulted in.
  //
  // Smaller allocations usually reuse freed memory, which has to be zeroed
  // anyway, and the calloc functions would zero their whole slot rather than
  // only `size` bytes, so `alloc()` and `memset()` are faster for them.
  if (partition_alloc::IsAlwaysDirectMapped(size)) {
    // Like `alloc()`, as `dealloc()` and `realloc()` expect.
    if (align <= alignof(std::max_align_t)) {
      return static_cast<unsigned char*>(
          allocator_shim::UncheckedCalloc(1, size));
    }
    // PartitionAlloc will crash if given an alignment larger than this.
    if (align <= partition_alloc::internal::kMaxSupportedAlignment) {
      return static_cast<unsigned char*>(
          allocator_shim::UncheckedAlignedCalloc(1, size, align));
    }
  }
#endif  // BUILDFLAG(RUST_ALLOCATOR_USES_PARTITION_ALLOC)
  unsigned char* p = alloc(size, align);
  if (p) {
    // SAFETY: `p` points to a newly allocated block of `size` bytes, so
    // zeroing `size` bytes is safe.
    BUILD_UNSAFE_BUFFERS(memset(p, 0, size));
  }
  return p;
#else
#error This configuration is not supported.
#endif
}

}  // namespace rust_allocator_internal
