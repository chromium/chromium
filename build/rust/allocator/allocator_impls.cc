// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/allocator/allocator_impls.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include <cstddef>
#include <cstring>

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
  // TODO(danakj): When RUST_ALLOCATOR_USES_PARTITION_ALLOC is true, it's
  // possible that a partition_alloc::UncheckedAllocZeroed() call would perform
  // better than partition_alloc::UncheckedAlloc() + memset. But there is no
  // such API today. See b/342251590.
  unsigned char* p = alloc(size, align);
  if (p) {
    memset(p, 0, size);
  }
  return p;
#else
#error This configuration is not supported.
#endif
}

}  // namespace rust_allocator_internal
