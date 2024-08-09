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
void* PartitionMalloc(size_t size, void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionMallocUnchecked(size_t size, void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionCalloc(size_t n, size_t size, void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionMemalign(size_t alignment, size_t size, void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionAlignedAlloc(size_t size, size_t alignment, void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionAlignedAllocUnchecked(size_t size,
                                     size_t alignment,
                                     void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionAlignedRealloc(void* address,
                              size_t size,
                              size_t alignment,
                              void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionAlignedReallocUnchecked(void* address,
                                       size_t size,
                                       size_t alignment,
                                       void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionRealloc(void* address, size_t size, void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void* PartitionReallocUnchecked(void* address, size_t size, void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void PartitionFree(void* object, void* context);

#if PA_BUILDFLAG(IS_APPLE)
PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void PartitionFreeDefiniteSize(void* address, size_t size, void* context);
#endif  // PA_BUILDFLAG(IS_APPLE)

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
size_t PartitionGetSizeEstimate(void* address, void* context);

#if PA_BUILDFLAG(IS_APPLE)
PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
size_t PartitionGoodSize(size_t size, void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
bool PartitionClaimedAddress(void* address, void* context);
#endif  // PA_BUILDFLAG(IS_APPLE)

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
unsigned PartitionBatchMalloc(size_t size,
                              void** results,
                              unsigned num_requested,
                              void* context);

PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void PartitionBatchFree(void** to_be_freed,
                        unsigned num_to_be_freed,
                        void* context);

#if PA_BUILDFLAG(IS_APPLE)
PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void PartitionTryFreeDefault(void* address, void* context);
#endif  // PA_BUILDFLAG(IS_APPLE)

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
