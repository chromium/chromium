// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc.h"

#include <string.h>

#include <memory>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/checked_ptr_support.h"
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_alloc_hooks.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/partition_stats.h"

namespace base {

#if ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR
namespace internal {
BASE_EXPORT PartitionTagWrapper g_checked_ptr_single_tag = {{},
                                                            kFixedTagValue,
                                                            {}};
}
#endif

void PartitionAllocGlobalInit(OomFunction on_out_of_memory) {
  // This is from page_allocator_constants.h and doesn't really fit here, but
  // there isn't a centralized initialization function in page_allocator.cc, so
  // there's no good place in that file to do a STATIC_ASSERT_OR_PA_CHECK.
  STATIC_ASSERT_OR_PA_CHECK((SystemPageSize() & (SystemPageSize() - 1)) == 0,
                            "SystemPageSize() must be power of 2");

  // Two partition pages are used as guard / metadata page so make sure the
  // super page size is bigger.
  STATIC_ASSERT_OR_PA_CHECK(PartitionPageSize() * 4 <= kSuperPageSize,
                            "ok super page size");
  STATIC_ASSERT_OR_PA_CHECK(!(kSuperPageSize % PartitionPageSize()),
                            "ok super page multiple");
  // Four system pages gives us room to hack out a still-guard-paged piece
  // of metadata in the middle of a guard partition page.
  STATIC_ASSERT_OR_PA_CHECK(SystemPageSize() * 4 <= PartitionPageSize(),
                            "ok partition page size");
  STATIC_ASSERT_OR_PA_CHECK(!(PartitionPageSize() % SystemPageSize()),
                            "ok partition page multiple");
  static_assert(sizeof(internal::PartitionPage<internal::ThreadSafe>) <=
                    kPageMetadataSize,
                "PartitionPage should not be too big");
  STATIC_ASSERT_OR_PA_CHECK(
      kPageMetadataSize * NumPartitionPagesPerSuperPage() <= SystemPageSize(),
      "page metadata fits in hole");

  // Limit to prevent callers accidentally overflowing an int size.
  STATIC_ASSERT_OR_PA_CHECK(
      MaxDirectMapped() <= (1UL << 31) + PageAllocationGranularity(),
      "maximum direct mapped allocation");

  // Check that some of our zanier calculations worked out as expected.
#if ENABLE_TAG_FOR_MTE_CHECKED_PTR
  static_assert(kSmallestBucket >= kAlignment, "generic smallest bucket");
#else
  static_assert(kSmallestBucket == kAlignment, "generic smallest bucket");
#endif
  static_assert(kMaxBucketed == 983040, "generic max bucketed");
  STATIC_ASSERT_OR_PA_CHECK(
      MaxSystemPagesPerSlotSpan() < (1 << 8),
      "System pages per slot span must be less than 128.");

  PA_DCHECK(on_out_of_memory);
  internal::g_oom_handling_function = on_out_of_memory;
}

void PartitionAllocGlobalUninitForTesting() {
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if (features::IsPartitionAllocGigaCageEnabled()) {
#if defined(PA_HAS_64_BITS_POINTERS)
    internal::PartitionAddressSpace::UninitForTesting();
#else
    internal::AddressPoolManager::GetInstance()->ResetForTesting();
#endif  // defined(PA_HAS_64_BITS_POINTERS)
  }
#endif  // !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  internal::g_oom_handling_function = nullptr;
}

namespace internal {

template <bool thread_safe>
PartitionAllocator<thread_safe>::~PartitionAllocator() {
  PartitionAllocMemoryReclaimer::Instance()->UnregisterPartition(
      &partition_root_);
}

template <bool thread_safe>
void PartitionAllocator<thread_safe>::init(PartitionOptions opts) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  PA_CHECK(opts.thread_cache == PartitionOptions::ThreadCache::kDisabled)
      << "Cannot use a thread cache when PartitionAlloc is malloc().";
#endif
  partition_root_.Init(opts);
  PartitionAllocMemoryReclaimer::Instance()->RegisterPartition(
      &partition_root_);
}

template PartitionAllocator<internal::ThreadSafe>::~PartitionAllocator();
template void PartitionAllocator<internal::ThreadSafe>::init(PartitionOptions);
template PartitionAllocator<internal::NotThreadSafe>::~PartitionAllocator();
template void PartitionAllocator<internal::NotThreadSafe>::init(
    PartitionOptions);

#if DCHECK_IS_ON()
void DCheckIfManagedByPartitionAllocNormalBuckets(const void* ptr) {
  PA_DCHECK(IsManagedByPartitionAllocNormalBuckets(ptr));
}
#endif

// Gets the offset from the beginning of the allocated slot, adjusted for cookie
// (if any).
// CAUTION! Use only for normal buckets. Using on direct-mapped allocations may
// lead to undefined behavior.
//
// This function is not a template, and can be used on either variant
// (thread-safe or not) of the allocator. This relies on the two PartitionRoot<>
// having the same layout, which is enforced by static_assert().
BASE_EXPORT size_t PartitionAllocGetSlotOffset(void* ptr) {
  internal::DCheckIfManagedByPartitionAllocNormalBuckets(ptr);
  auto* slot_span =
      internal::PartitionAllocGetSlotSpanForSizeQuery<internal::ThreadSafe>(
          ptr);
  auto* root = PartitionRoot<internal::ThreadSafe>::FromSlotSpan(slot_span);
  // The only allocations that don't use tag/ref-count are allocated outside of
  // GigaCage, hence we'd never get here in the `allow_extras = false` case.
  PA_DCHECK(root->allow_extras);
  ptr = root->AdjustPointerForExtrasSubtract(ptr);

  // Get the offset from the beginning of the slot span.
  uintptr_t ptr_addr = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t slot_span_start = reinterpret_cast<uintptr_t>(
      internal::SlotSpanMetadata<internal::ThreadSafe>::ToPointer(slot_span));
  size_t offset_in_slot_span = ptr_addr - slot_span_start;

  return slot_span->bucket->GetSlotOffset(offset_in_slot_span);
}

}  // namespace internal

}  // namespace base
