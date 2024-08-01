// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_
#define PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/shim/allocator_shim.h"

namespace allocator_shim {

struct AllocatorDispatch;

namespace internal {

class PA_COMPONENT_EXPORT(ALLOCATOR_SHIM) PartitionAllocMalloc {
 public:
  // Returns true if ConfigurePartitions() has completed, meaning that the
  // allocators are effectively set in stone.
  static bool AllocatorConfigurationFinalized();

  static partition_alloc::PartitionRoot* Allocator();
  // May return |nullptr|, will never return the same pointer as |Allocator()|.
  static partition_alloc::PartitionRoot* OriginalAllocator();
};

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionMalloc(const AllocatorDispatch*, size_t size, void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionMallocUnchecked(const AllocatorDispatch*,
                               size_t size,
                               void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionCalloc(const AllocatorDispatch*,
                      size_t n,
                      size_t size,
                      void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionMemalign(const AllocatorDispatch*,
                        size_t alignment,
                        size_t size,
                        void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionAlignedAlloc(const AllocatorDispatch* dispatch,
                            size_t size,
                            size_t alignment,
                            void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionAlignedAllocUnchecked(const AllocatorDispatch* dispatch,
                                     size_t size,
                                     size_t alignment,
                                     void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionAlignedRealloc(const AllocatorDispatch* dispatch,
                              void* address,
                              size_t size,
                              size_t alignment,
                              void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionAlignedReallocUnchecked(const AllocatorDispatch* dispatch,
                                       void* address,
                                       size_t size,
                                       size_t alignment,
                                       void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionRealloc(const AllocatorDispatch*,
                       void* address,
                       size_t size,
                       void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionReallocUnchecked(const AllocatorDispatch*,
                                void* address,
                                size_t size,
                                void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void PartitionFree(const AllocatorDispatch*, void* object, void* context);

#if PA_BUILDFLAG(IS_APPLE)
PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void PartitionFreeDefiniteSize(const AllocatorDispatch*,
                               void* address,
                               size_t size,
                               void* context);
#endif  // PA_BUILDFLAG(IS_APPLE)

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
size_t PartitionGetSizeEstimate(const AllocatorDispatch*,
                                void* address,
                                void* context);

#if PA_BUILDFLAG(IS_APPLE)
PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
size_t PartitionGoodSize(const AllocatorDispatch*, size_t size, void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
bool PartitionClaimedAddress(const AllocatorDispatch*,
                             void* address,
                             void* context);
#endif  // PA_BUILDFLAG(IS_APPLE)

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
unsigned PartitionBatchMalloc(const AllocatorDispatch*,
                              size_t size,
                              void** results,
                              unsigned num_requested,
                              void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void PartitionBatchFree(const AllocatorDispatch*,
                        void** to_be_freed,
                        unsigned num_to_be_freed,
                        void* context);

#if PA_BUILDFLAG(IS_APPLE)
PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void PartitionTryFreeDefault(const AllocatorDispatch*,
                             void* address,
                             void* context);
#endif  // PA_BUILDFLAG(IS_APPLE)

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

}  // namespace internal

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// Provide a ConfigurePartitions() helper, to mimic what Chromium uses. This way
// we're making it more resilient to ConfigurePartitions() interface changes, so
// that we don't have to modify multiple callers. This is particularly important
// when callers are in a different repo, like PDFium or Dawn.
// -----------------------------------------------------------------------------
// DO NOT MODIFY this signature. This is meant for partition_alloc's embedders
// only, so that partition_alloc can evolve without breaking them.
// Chromium/PartitionAlloc are part of the same repo, they must not depend on
// this function. They should call ConfigurePartitions() directly.
PA_ALWAYS_INLINE void ConfigurePartitionsForTesting() {
  auto enable_brp = allocator_shim::EnableBrp(true);

  // Embedders's tests might benefit from MTE checks. However, this is costly
  // and shouldn't be used in benchmarks.
  auto enable_memory_tagging = allocator_shim::EnableMemoryTagging(
      PA_BUILDFLAG(HAS_MEMORY_TAGGING) && PA_BUILDFLAG(DCHECKS_ARE_ON));

  // Since the only user of this function is a test function, we use
  // synchronous reporting mode, if MTE is enabled.
  auto memory_tagging_reporting_mode =
      enable_memory_tagging
          ? partition_alloc::TagViolationReportingMode::kSynchronous
          : partition_alloc::TagViolationReportingMode::kDisabled;
  auto distribution = BucketDistribution::kNeutral;
  auto scheduler_loop_quarantine = SchedulerLoopQuarantine(false);
  size_t scheduler_loop_quarantine_capacity_in_bytes = 0;
  auto zapping_by_free_flags = ZappingByFreeFlags(false);
  auto use_pool_offset_freelists = UsePoolOffsetFreelists(true);
  auto use_small_single_slot_spans = UseSmallSingleSlotSpans(true);

  ConfigurePartitions(
      enable_brp, enable_memory_tagging, memory_tagging_reporting_mode,
      distribution, scheduler_loop_quarantine,
      scheduler_loop_quarantine_capacity_in_bytes, zapping_by_free_flags,
      use_pool_offset_freelists, use_small_single_slot_spans);
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace allocator_shim

#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

#endif  // PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_
