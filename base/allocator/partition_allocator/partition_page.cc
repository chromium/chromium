// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_page.h"

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/bits.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"

namespace base {
namespace internal {

namespace {

template <bool thread_safe>
ALWAYS_INLINE DeferredUnmap
PartitionDirectUnmap(SlotSpanMetadata<thread_safe>* slot_span) {
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(slot_span);
  root->lock_.AssertAcquired();
  auto* extent = PartitionDirectMapExtent<thread_safe>::FromSlotSpan(slot_span);

  // Maintain the doubly-linked list of all direct mappings.
  if (extent->prev_extent) {
    PA_DCHECK(extent->prev_extent->next_extent == extent);
    extent->prev_extent->next_extent = extent->next_extent;
  } else {
    root->direct_map_list = extent->next_extent;
  }
  if (extent->next_extent) {
    PA_DCHECK(extent->next_extent->prev_extent == extent);
    extent->next_extent->prev_extent = extent->prev_extent;
  }

  // The actual decommit is deferred, when releasing the reserved memory region.
  root->DecreaseCommittedPages(slot_span->bucket->slot_size);

  size_t reserved_size =
      extent->map_size +
      PartitionRoot<thread_safe>::GetDirectMapMetadataAndGuardPagesSize();
  PA_DCHECK(!(reserved_size & PageAllocationGranularityOffsetMask()));
  PA_DCHECK(root->total_size_of_direct_mapped_pages >= reserved_size);
  root->total_size_of_direct_mapped_pages -= reserved_size;
  PA_DCHECK(!(reserved_size & PageAllocationGranularityOffsetMask()));

  char* ptr = reinterpret_cast<char*>(
      SlotSpanMetadata<thread_safe>::ToSlotSpanStartPtr(slot_span));
  // Account for the mapping starting a partition page before the actual
  // allocation address.
  ptr -= PartitionPageSize();
  return {ptr, reserved_size};
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRegisterEmptySlotSpan(
    SlotSpanMetadata<thread_safe>* slot_span) {
  PA_DCHECK(slot_span->is_empty());
  PartitionRoot<thread_safe>* root =
      PartitionRoot<thread_safe>::FromSlotSpan(slot_span);
  root->lock_.AssertAcquired();

  // If the slot span is already registered as empty, give it another life.
  if (slot_span->empty_cache_index != -1) {
    PA_DCHECK(slot_span->empty_cache_index >= 0);
    PA_DCHECK(static_cast<unsigned>(slot_span->empty_cache_index) <
              kMaxFreeableSpans);
    PA_DCHECK(root->global_empty_slot_span_ring[slot_span->empty_cache_index] ==
              slot_span);
    root->global_empty_slot_span_ring[slot_span->empty_cache_index] = nullptr;
  }

  int16_t current_index = root->global_empty_slot_span_ring_index;
  SlotSpanMetadata<thread_safe>* slot_span_to_decommit =
      root->global_empty_slot_span_ring[current_index];
  // The slot span might well have been re-activated, filled up, etc. before we
  // get around to looking at it here.
  if (slot_span_to_decommit)
    slot_span_to_decommit->DecommitIfPossible(root);

  // We put the empty slot span on our global list of "slot spans that were once
  // empty". thus providing it a bit of breathing room to get re-used before
  // we really free it. This improves performance, particularly on Mac OS X
  // which has subpar memory management performance.
  root->global_empty_slot_span_ring[current_index] = slot_span;
  slot_span->empty_cache_index = current_index;
  ++current_index;
  if (current_index == kMaxFreeableSpans)
    current_index = 0;
  root->global_empty_slot_span_ring_index = current_index;
}

}  // namespace

// static
template <bool thread_safe>
SlotSpanMetadata<thread_safe>
    SlotSpanMetadata<thread_safe>::sentinel_slot_span_;

// static
template <bool thread_safe>
SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::get_sentinel_slot_span() {
  return &sentinel_slot_span_;
}

template <bool thread_safe>
SlotSpanMetadata<thread_safe>::SlotSpanMetadata(
    PartitionBucket<thread_safe>* bucket)
    : bucket(bucket), can_store_raw_size(bucket->CanStoreRawSize()) {}

template <bool thread_safe>
DeferredUnmap SlotSpanMetadata<thread_safe>::FreeSlowPath() {
#if DCHECK_IS_ON()
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(this);
  root->lock_.AssertAcquired();
#endif
  PA_DCHECK(this != get_sentinel_slot_span());
  if (LIKELY(num_allocated_slots == 0)) {
    // Slot span became fully unused.
    if (UNLIKELY(bucket->is_direct_mapped())) {
      return PartitionDirectUnmap(this);
    }
#if DCHECK_IS_ON()
    freelist_head->CheckFreeList();
#endif
    // If it's the current active slot span, change it. We bounce the slot span
    // to the empty list as a force towards defragmentation.
    if (LIKELY(this == bucket->active_slot_spans_head))
      bucket->SetNewActiveSlotSpan();
    PA_DCHECK(bucket->active_slot_spans_head != this);

    if (CanStoreRawSize())
      SetRawSize(0);

    PartitionRegisterEmptySlotSpan(this);
  } else {
    PA_DCHECK(!bucket->is_direct_mapped());
    // Ensure that the slot span is full. That's the only valid case if we
    // arrive here.
    PA_DCHECK(num_allocated_slots < 0);
    // A transition of num_allocated_slots from 0 to -1 is not legal, and
    // likely indicates a double-free.
    PA_CHECK(num_allocated_slots != -1);
    num_allocated_slots = -num_allocated_slots - 2;
    PA_DCHECK(num_allocated_slots == bucket->get_slots_per_span() - 1);
    // Fully used slot span became partially used. It must be put back on the
    // non-full list. Also make it the current slot span to increase the
    // chances of it being filled up again. The old current slot span will be
    // the next slot span.
    PA_DCHECK(!next_slot_span);
    if (LIKELY(bucket->active_slot_spans_head != get_sentinel_slot_span()))
      next_slot_span = bucket->active_slot_spans_head;
    bucket->active_slot_spans_head = this;
    --bucket->num_full_slot_spans;
    // Special case: for a partition slot span with just a single slot, it may
    // now be empty and we want to run it through the empty logic.
    if (UNLIKELY(num_allocated_slots == 0))
      return FreeSlowPath();
  }
  return {};
}

template <bool thread_safe>
void SlotSpanMetadata<thread_safe>::Decommit(PartitionRoot<thread_safe>* root) {
  root->lock_.AssertAcquired();
  PA_DCHECK(is_empty());
  PA_DCHECK(!bucket->is_direct_mapped());
  void* slot_span_start = SlotSpanMetadata::ToSlotSpanStartPtr(this);
  // If lazy commit is enabled, only provisioned slots are committed.
  size_t size_to_decommit =
      root->use_lazy_commit
          ? bits::AlignUp(GetProvisionedSize(), SystemPageSize())
          : bucket->get_bytes_per_span();

  // Not decommitted slot span must've had at least 1 allocation.
  PA_DCHECK(size_to_decommit > 0);
  root->DecommitSystemPagesForData(slot_span_start, size_to_decommit,
                                   PageKeepPermissionsIfPossible);

  // We actually leave the decommitted slot span in the active list. We'll sweep
  // it on to the decommitted list when we next walk the active list.
  // Pulling this trick enables us to use a singly-linked list for all
  // cases, which is critical in keeping the slot span metadata structure down
  // to 32 bytes in size.
  SetFreelistHead(nullptr);
  num_unprovisioned_slots = 0;
  PA_DCHECK(is_decommitted());
  PA_DCHECK(bucket);
}

template <bool thread_safe>
void SlotSpanMetadata<thread_safe>::DecommitIfPossible(
    PartitionRoot<thread_safe>* root) {
  root->lock_.AssertAcquired();
  PA_DCHECK(empty_cache_index >= 0);
  PA_DCHECK(static_cast<unsigned>(empty_cache_index) < kMaxFreeableSpans);
  PA_DCHECK(this == root->global_empty_slot_span_ring[empty_cache_index]);
  empty_cache_index = -1;
  if (is_empty())
    Decommit(root);
}

void DeferredUnmap::Unmap() {
  PA_DCHECK(ptr && size > 0);
  if (features::IsPartitionAllocGigaCageEnabled()) {
    // Currently this function is only called for direct-mapped allocations,
    // which always belong to the non-BRP pool, provided that GigaCage is used.
    // TODO(bartekn): Add a !"is in normal buckets" DCHECK.
    PA_DCHECK(IsManagedByPartitionAllocNonBRPPool(ptr));
    internal::AddressPoolManager::GetInstance()->UnreserveAndDecommit(
        internal::GetNonBRPPool(), ptr, size);
  } else {
    FreePages(ptr, size);
  }
}

template struct SlotSpanMetadata<ThreadSafe>;
template struct SlotSpanMetadata<NotThreadSafe>;

}  // namespace internal
}  // namespace base
