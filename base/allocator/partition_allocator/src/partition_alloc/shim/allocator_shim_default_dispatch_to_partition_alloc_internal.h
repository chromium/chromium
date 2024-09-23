// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_INTERNAL_H_
#define PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_INTERNAL_H_

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"

namespace allocator_shim::internal {

// Only allocator_shim component must include this header, because
// PartitionMalloc, PartitionMallocUnchecked, ... are DLL-exported when
// is_component_build=true and is_win=true. In the case, the other component
// needs to import the symbols from allocator_shim.dll...so, not constexpr.
inline constexpr AllocatorDispatch kPartitionAllocDispatch = {
    &allocator_shim::internal::PartitionMalloc,  // alloc_function
    &allocator_shim::internal::
        PartitionMallocUnchecked,  // alloc_unchecked_function
    &allocator_shim::internal::
        PartitionCalloc,  // alloc_zero_initialized_function
    &allocator_shim::internal::PartitionMemalign,  // alloc_aligned_function
    &allocator_shim::internal::PartitionRealloc,   // realloc_function
    &allocator_shim::internal::
        PartitionReallocUnchecked,             // realloc_unchecked_function
    &allocator_shim::internal::PartitionFree,  // free_function
    &allocator_shim::internal::
        PartitionGetSizeEstimate,  // get_size_estimate_function
#if PA_BUILDFLAG(IS_APPLE)
    &allocator_shim::internal::PartitionGoodSize,        // good_size
    &allocator_shim::internal::PartitionClaimedAddress,  // claimed_address
#else
    nullptr,  // good_size
    nullptr,  // claimed_address
#endif
    &allocator_shim::internal::PartitionBatchMalloc,  // batch_malloc_function
    &allocator_shim::internal::PartitionBatchFree,    // batch_free_function
#if PA_BUILDFLAG(IS_APPLE)
    // On Apple OSes, free_definite_size() is always called from free(), since
    // get_size_estimate() is used to determine whether an allocation belongs to
    // the current zone. It makes sense to optimize for it.
    &allocator_shim::internal::PartitionFreeDefiniteSize,
    // On Apple OSes, try_free_default() is sometimes called as an optimization
    // of free().
    &allocator_shim::internal::PartitionTryFreeDefault,
#else
    nullptr,  // free_definite_size_function
    nullptr,  // try_free_default_function
#endif
    &allocator_shim::internal::
        PartitionAlignedAlloc,  // aligned_malloc_function
    &allocator_shim::internal::
        PartitionAlignedAllocUnchecked,  // aligned_malloc_unchecked_function
    &allocator_shim::internal::
        PartitionAlignedRealloc,  // aligned_realloc_function
    &allocator_shim::internal::
        PartitionAlignedReallocUnchecked,  // aligned_realloc_unchecked_function
    &allocator_shim::internal::PartitionFree,  // aligned_free_function
    nullptr,                                   // next
};

}  // namespace allocator_shim::internal

#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

#endif  // PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_INTERNAL_H_
