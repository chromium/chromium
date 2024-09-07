// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc.h"

#include <cstdint>
#include <cstring>
#include <memory>

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/memory_reclaimer.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc_hooks.h"
#include "partition_alloc/partition_direct_map_extent.h"
#include "partition_alloc/partition_oom.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/partition_stats.h"

namespace partition_alloc {

void PartitionAllocGlobalInit(OomFunction on_out_of_memory) {
  // This is from page_allocator_constants.h and doesn't really fit here, but
  // there isn't a centralized initialization function in page_allocator.cc, so
  // there's no good place in that file to do a STATIC_ASSERT_OR_PA_CHECK.
  STATIC_ASSERT_OR_PA_CHECK(
      (internal::SystemPageSize() & internal::SystemPageOffsetMask()) == 0,
      "SystemPageSize() must be power of 2");

  // Two partition pages are used as guard / metadata page so make sure the
  // super page size is bigger.
  STATIC_ASSERT_OR_PA_CHECK(
      internal::PartitionPageSize() * 4 <= internal::kSuperPageSize,
      "ok super page size");
  STATIC_ASSERT_OR_PA_CHECK(
      (internal::kSuperPageSize & internal::SystemPageOffsetMask()) == 0,
      "ok super page multiple");
  // Four system pages gives us room to hack out a still-guard-paged piece
  // of metadata in the middle of a guard partition page.
  STATIC_ASSERT_OR_PA_CHECK(
      internal::SystemPageSize() * 4 <= internal::PartitionPageSize(),
      "ok partition page size");
  STATIC_ASSERT_OR_PA_CHECK(
      (internal::PartitionPageSize() & internal::SystemPageOffsetMask()) == 0,
      "ok partition page multiple");
  static_assert(
      sizeof(
          internal::PartitionPageMetadata<internal::MetadataKind::kReadOnly>) <=
              internal::kPageMetadataSize &&
          sizeof(internal::PartitionPageMetadata<
                 internal::MetadataKind::kWritable>) <=
              internal::kPageMetadataSize,
      "PartitionPage should not be too big");
  STATIC_ASSERT_OR_PA_CHECK(
      internal::kPageMetadataSize * internal::NumPartitionPagesPerSuperPage() <=
          internal::SystemPageSize(),
      "page metadata fits in hole");

  // Limit to prevent callers accidentally overflowing an int size.
  STATIC_ASSERT_OR_PA_CHECK(
      internal::MaxDirectMapped() <=
          (1UL << 31) + internal::DirectMapAllocationGranularity(),
      "maximum direct mapped allocation");

  // Check that some of our zanier calculations worked out as expected.
  static_assert(internal::kSmallestBucket == internal::kAlignment,
                "generic smallest bucket");
  static_assert(internal::kMaxBucketed == 983040, "generic max bucketed");
  STATIC_ASSERT_OR_PA_CHECK(
      internal::MaxSystemPagesPerRegularSlotSpan() <= 16,
      "System pages per slot span must be no greater than 16.");

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  STATIC_ASSERT_OR_PA_CHECK(
      internal::GetInSlotMetadataIndexMultiplierShift() <
          std::numeric_limits<size_t>::max() / 2,
      "Calculation in GetInSlotMetadataIndexMultiplierShift() must not "
      "underflow.");
  // Check that the GetInSlotMetadataIndexMultiplierShift() calculation is
  // correct.
  STATIC_ASSERT_OR_PA_CHECK(
      (1 << internal::GetInSlotMetadataIndexMultiplierShift()) ==
          (internal::SystemPageSize() /
           (sizeof(internal::InSlotMetadata) *
            (internal::kSuperPageSize / internal::SystemPageSize()))),
      "Bitshift must match the intended multiplication.");
  STATIC_ASSERT_OR_PA_CHECK(
      ((sizeof(internal::InSlotMetadata) *
        (internal::kSuperPageSize / internal::SystemPageSize()))
       << internal::GetInSlotMetadataIndexMultiplierShift()) <=
          internal::SystemPageSize(),
      "InSlotMetadata table size must be smaller than or equal to "
      "<= SystemPageSize().");
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

  PA_DCHECK(on_out_of_memory);
  internal::g_oom_handling_function = on_out_of_memory;
}

void PartitionAllocGlobalUninitForTesting() {
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  internal::PartitionAddressSpace::UninitThreadIsolatedPoolForTesting();
#endif
  internal::g_oom_handling_function = nullptr;
}

PartitionAllocator::PartitionAllocator() = default;

PartitionAllocator::~PartitionAllocator() {
  MemoryReclaimer::Instance()->UnregisterPartition(&partition_root_);
}

void PartitionAllocator::init(PartitionOptions opts) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  PA_CHECK(opts.thread_cache == PartitionOptions::kDisabled)
      << "Cannot use a thread cache when PartitionAlloc is malloc().";
#endif
  partition_root_.Init(opts);
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // The MemoryReclaimer won't have write access to the partition, so skip
  // registration.
  const bool use_memory_reclaimer = !opts.thread_isolation.enabled;
#else
  constexpr bool use_memory_reclaimer = true;
#endif
  if (use_memory_reclaimer) {
    MemoryReclaimer::Instance()->RegisterPartition(&partition_root_);
  }
}

}  // namespace partition_alloc
