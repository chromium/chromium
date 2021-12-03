// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_page.h"

#include <algorithm>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/bits.h"
#include "base/dcheck_is_on.h"
#include "base/memory/tagging.h"

namespace base {
namespace internal {

namespace {

void UnmapNow(uintptr_t reservation_start,
              size_t reservation_size,
              pool_handle pool);

template <bool thread_safe>
ALWAYS_INLINE void PartitionDirectUnmap(
    SlotSpanMetadata<thread_safe>* slot_span) {
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

  // The actual decommit is deferred below after releasing the lock.
  root->DecreaseCommittedPages(slot_span->bucket->slot_size);

  size_t reservation_size = extent->reservation_size;
  PA_DCHECK(!(reservation_size & DirectMapAllocationGranularityOffsetMask()));
  PA_DCHECK(root->total_size_of_direct_mapped_pages >= reservation_size);
  root->total_size_of_direct_mapped_pages -= reservation_size;

  uintptr_t reservation_start = reinterpret_cast<uintptr_t>(
      SlotSpanMetadata<thread_safe>::ToSlotSpanStartPtr(slot_span));
  // The mapping may start at an unspecified location within a super page, but
  // we always reserve memory aligned to super page size.
  reservation_start = bits::AlignDown(reservation_start, kSuperPageSize);

  // All the metadata have been updated above, in particular the mapping has
  // been unlinked. We can safely release the memory outside the lock, which is
  // important as decommitting memory can be expensive.
  //
  // This can create a fake "address space exhaustion" OOM, in the case where
  // e.g. a large allocation is freed on a thread, and another large one is made
  // from another *before* UnmapNow() has finished running. In this case the
  // second one may not find enough space in the GigaCage, and fail. This is
  // expected to be very rare though, and likely preferable to holding the lock
  // while releasing the address space.
  ScopedUnlockGuard<thread_safe> unlock{root->lock_};
  ScopedSyscallTimer<thread_safe> timer{root};
  UnmapNow(reservation_start, reservation_size, root->ChoosePool());
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRegisterEmptySlotSpan(
    SlotSpanMetadata<thread_safe>* slot_span) {
  PA_DCHECK(slot_span->is_empty());
  PartitionRoot<thread_safe>* root =
      PartitionRoot<thread_safe>::FromSlotSpan(slot_span);
  root->lock_.AssertAcquired();

  root->empty_slot_spans_dirty_bytes +=
      base::bits::AlignUp(slot_span->GetProvisionedSize(), SystemPageSize());

  slot_span->ToSuperPageExtent()->DecrementNumberOfNonemptySlotSpans();

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

  // Avoid wasting too much memory on empty slot spans. Note that we only divide
  // by powers of two, since division can be very slow, and this path is taken
  // for every single-slot slot span deallocation.
  //
  // Empty slot spans are also all decommitted with MemoryReclaimer, but it may
  // never run, be delayed arbitrarily, and/or miss large memory spikes.
  size_t max_empty_dirty_bytes =
      root->total_size_of_committed_pages.load(std::memory_order_relaxed) >>
      root->max_empty_slot_spans_dirty_bytes_shift;
  if (root->empty_slot_spans_dirty_bytes > max_empty_dirty_bytes) {
    root->ShrinkEmptySlotSpansRing(std::min(
        root->empty_slot_spans_dirty_bytes / 2, max_empty_dirty_bytes));
  }
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
void SlotSpanMetadata<thread_safe>::FreeSlowPath(size_t number_of_freed) {
#if DCHECK_IS_ON()
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(this);
  root->lock_.AssertAcquired();
#endif
  PA_DCHECK(this != get_sentinel_slot_span());
  if (LIKELY(num_allocated_slots == 0)) {
    // Slot span became fully unused.
    if (UNLIKELY(bucket->is_direct_mapped())) {
      PartitionDirectUnmap(this);
      return;
    }
#if DCHECK_IS_ON()
    freelist_head->CheckFreeList(bucket->slot_size);
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
    // A transition of num_allocated_slots from 0 to a negative value is not
    // legal, and likely indicates a double-free.
    PA_CHECK(static_cast<intptr_t>(num_allocated_slots) <
             -static_cast<intptr_t>(number_of_freed));
    num_allocated_slots = -num_allocated_slots - 2 * number_of_freed;
    PA_DCHECK(static_cast<size_t>(num_allocated_slots) ==
              bucket->get_slots_per_span() - number_of_freed);
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
      FreeSlowPath(number_of_freed /*ignored*/);
  }
}

template <bool thread_safe>
void SlotSpanMetadata<thread_safe>::Decommit(PartitionRoot<thread_safe>* root) {
  root->lock_.AssertAcquired();
  PA_DCHECK(is_empty());
  PA_DCHECK(!bucket->is_direct_mapped());
  uintptr_t slot_span_start =
      reinterpret_cast<uintptr_t>(SlotSpanMetadata::ToSlotSpanStartPtr(this));
  // If lazy commit is enabled, only provisioned slots are committed.
  size_t dirty_size = bits::AlignUp(GetProvisionedSize(), SystemPageSize());
  size_t size_to_decommit =
      root->use_lazy_commit ? dirty_size : bucket->get_bytes_per_span();

  PA_DCHECK(root->empty_slot_spans_dirty_bytes >= dirty_size);
  root->empty_slot_spans_dirty_bytes -= dirty_size;

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

template <bool thread_safe>
void SlotSpanMetadata<thread_safe>::SortFreelist() {
  std::bitset<kMaxSlotsPerSlotSpan> free_slots;
  char* slot_span_base = reinterpret_cast<char*>(ToSlotSpanStartPtr(this));

  size_t num_provisioned_slots =
      bucket->get_slots_per_span() - num_unprovisioned_slots;
  PA_CHECK(num_unprovisioned_slots <= kMaxSlotsPerSlotSpan);

  size_t slot_size = bucket->slot_size;
  for (PartitionFreelistEntry* head = freelist_head; head;
       head = head->GetNext(slot_size)) {
    size_t offset_in_slot_span =
        reinterpret_cast<char*>(memory::UnmaskPtr(head)) - slot_span_base;
    size_t slot_number = bucket->GetSlotNumber(offset_in_slot_span);
    PA_DCHECK(slot_number < num_provisioned_slots);
    free_slots[slot_number] = true;
  }

  PartitionFreelistEntry* back = nullptr;
  PartitionFreelistEntry* head = nullptr;

  for (size_t slot_number = 0; slot_number < num_provisioned_slots;
       slot_number++) {
    if (free_slots[slot_number]) {
      char* slot_address =
          memory::RemaskPtr(slot_span_base + (slot_size * slot_number));
      auto* entry = new (slot_address) PartitionFreelistEntry();

      if (!head)
        head = entry;
      else
        back->SetNext(entry);

      back = entry;
    }
  }
  SetFreelistHead(head);
  freelist_is_sorted = true;
}

namespace {
void UnmapNow(uintptr_t reservation_start,
              size_t reservation_size,
              pool_handle pool) {
  PA_DCHECK(reservation_start && reservation_size > 0);
#if DCHECK_IS_ON()
  // When USE_BACKUP_REF_PTR is off, BRP pool isn't used.
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  if (pool == GetBRPPool()) {
    // In 32-bit mode, the beginning of a reservation may be excluded from the
    // BRP pool, so shift the pointer. Other pools don't have this logic.
    PA_DCHECK(IsManagedByPartitionAllocBRPPool(
#if defined(PA_HAS_64_BITS_POINTERS)
        reservation_start
#else
        reservation_start +
        AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap *
            AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap
#endif
        ));
  } else
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  {
    PA_DCHECK(pool == GetRegularPool() ||
              (IsConfigurablePoolAvailable() && pool == GetConfigurablePool()));
    // Non-BRP pools don't need adjustment that BRP needs in 32-bit mode.
    PA_DCHECK(IsManagedByPartitionAllocRegularPool(reservation_start) ||
              IsManagedByPartitionAllocConfigurablePool(reservation_start));
  }
#endif  // DCHECK_IS_ON()

  PA_DCHECK((reservation_start & kSuperPageOffsetMask) == 0);
  uintptr_t reservation_end = reservation_start + reservation_size;
  auto* offset_ptr = ReservationOffsetPointer(reservation_start);
  // Reset the offset table entries for the given memory before unreserving
  // it. Since the memory is not unreserved and not available for other
  // threads, the table entries for the memory are not modified by other
  // threads either. So we can update the table entries without race
  // condition.
  uint16_t i = 0;
  for (uintptr_t address = reservation_start; address < reservation_end;
       address += kSuperPageSize) {
    PA_DCHECK(offset_ptr < GetReservationOffsetTableEnd(address));
    PA_DCHECK(*offset_ptr == i++);
    *offset_ptr++ = kOffsetTagNotAllocated;
  }

#if !defined(PA_HAS_64_BITS_POINTERS)
  AddressPoolManager::GetInstance()->MarkUnused(pool, reservation_start,
                                                reservation_size);
#endif

  // After resetting the table entries, unreserve and decommit the memory.
  AddressPoolManager::GetInstance()->UnreserveAndDecommit(
      pool, reinterpret_cast<void*>(reservation_start), reservation_size);
}
}  // namespace

template struct SlotSpanMetadata<ThreadSafe>;
template struct SlotSpanMetadata<NotThreadSafe>;

}  // namespace internal
}  // namespace base
