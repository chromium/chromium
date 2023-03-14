// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_page.h"

#include <algorithm>
#include <cstdint>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/freeslot_bitmap.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/tagging.h"

namespace partition_alloc::internal {

namespace {

void UnmapNow(uintptr_t reservation_start,
              size_t reservation_size,
              pool_handle pool);

template <bool thread_safe>
PA_ALWAYS_INLINE void PartitionDirectUnmap(
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

  uintptr_t reservation_start =
      SlotSpanMetadata<thread_safe>::ToSlotSpanStart(slot_span);
  // The mapping may start at an unspecified location within a super page, but
  // we always reserve memory aligned to super page size.
  reservation_start = base::bits::AlignDown(reservation_start, kSuperPageSize);

  // All the metadata have been updated above, in particular the mapping has
  // been unlinked. We can safely release the memory outside the lock, which is
  // important as decommitting memory can be expensive.
  //
  // This can create a fake "address space exhaustion" OOM, in the case where
  // e.g. a large allocation is freed on a thread, and another large one is made
  // from another *before* UnmapNow() has finished running. In this case the
  // second one may not find enough space in the pool, and fail. This is
  // expected to be very rare though, and likely preferable to holding the lock
  // while releasing the address space.
  ScopedUnlockGuard unlock{root->lock_};
  ScopedSyscallTimer timer{root};
  UnmapNow(reservation_start, reservation_size, root->ChoosePool());
}

}  // namespace

template <bool thread_safe>
PA_ALWAYS_INLINE void SlotSpanMetadata<thread_safe>::RegisterEmpty() {
  PA_DCHECK(is_empty());
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(this);
  root->lock_.AssertAcquired();

  root->empty_slot_spans_dirty_bytes +=
      base::bits::AlignUp(GetProvisionedSize(), SystemPageSize());

  ToSuperPageExtent()->DecrementNumberOfNonemptySlotSpans();

  // If the slot span is already registered as empty, give it another life.
  if (in_empty_cache_) {
    PA_DCHECK(empty_cache_index_ < kMaxFreeableSpans);
    PA_DCHECK(root->global_empty_slot_span_ring[empty_cache_index_] == this);
    root->global_empty_slot_span_ring[empty_cache_index_] = nullptr;
  }

  int16_t current_index = root->global_empty_slot_span_ring_index;
  SlotSpanMetadata<thread_safe>* slot_span_to_decommit =
      root->global_empty_slot_span_ring[current_index];
  // The slot span might well have been re-activated, filled up, etc. before we
  // get around to looking at it here.
  if (slot_span_to_decommit) {
    slot_span_to_decommit->DecommitIfPossible(root);
  }

  // We put the empty slot span on our global list of "slot spans that were once
  // empty", thus providing it a bit of breathing room to get re-used before we
  // really free it. This reduces the number of system calls. Otherwise any
  // free() from a single-slot slot span would lead to a syscall, for instance.
  root->global_empty_slot_span_ring[current_index] = this;
  empty_cache_index_ = current_index;
  in_empty_cache_ = 1;
  ++current_index;
  if (current_index == root->global_empty_slot_span_ring_size) {
    current_index = 0;
  }
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
// static
template <bool thread_safe>
const SlotSpanMetadata<thread_safe>
    SlotSpanMetadata<thread_safe>::sentinel_slot_span_;

// static
template <bool thread_safe>
const SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::get_sentinel_slot_span() {
  return &sentinel_slot_span_;
}

// static
template <bool thread_safe>
SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::get_sentinel_slot_span_non_const() {
  return const_cast<SlotSpanMetadata<thread_safe>*>(&sentinel_slot_span_);
}

template <bool thread_safe>
SlotSpanMetadata<thread_safe>::SlotSpanMetadata(
    PartitionBucket<thread_safe>* bucket)
    : bucket(bucket), can_store_raw_size_(bucket->CanStoreRawSize()) {}

template <bool thread_safe>
void SlotSpanMetadata<thread_safe>::FreeSlowPath(size_t number_of_freed) {
#if BUILDFLAG(PA_DCHECK_IS_ON)
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(this);
  root->lock_.AssertAcquired();
#endif
  PA_DCHECK(this != get_sentinel_slot_span());

  // The caller has already modified |num_allocated_slots|. It is a
  // responsibility of this function to react to it, and update the state. We
  // can get here only if the slot span is marked full and/or is now empty. Both
  // are possible at the same time, which can happen when the caller lowered
  // |num_allocated_slots| from "all" to 0 (common for single-slot spans). First
  // execute the "is marked full" path, as it sets up |active_slot_spans_head|
  // in a way later needed for the "is empty" path.
  if (marked_full) {
    // Direct map slot spans aren't added to any lists, hence never marked full.
    PA_DCHECK(!bucket->is_direct_mapped());
    // Double check that the slot span was full.
    PA_DCHECK(num_allocated_slots ==
              bucket->get_slots_per_span() - number_of_freed);
    marked_full = 0;
    // Fully used slot span became partially used. It must be put back on the
    // non-full list. Also make it the current slot span to increase the
    // chances of it being filled up again. The old current slot span will be
    // the next slot span.
    PA_DCHECK(!next_slot_span);
    if (PA_LIKELY(bucket->active_slot_spans_head != get_sentinel_slot_span())) {
      next_slot_span = bucket->active_slot_spans_head;
    }
    bucket->active_slot_spans_head = this;
    PA_CHECK(bucket->num_full_slot_spans);  // Underflow.
    --bucket->num_full_slot_spans;
  }

  if (PA_LIKELY(num_allocated_slots == 0)) {
    // Slot span became fully unused.
    if (PA_UNLIKELY(bucket->is_direct_mapped())) {
      PartitionDirectUnmap(this);
      return;
    }
#if BUILDFLAG(PA_DCHECK_IS_ON)
    freelist_head->CheckFreeList(bucket->slot_size);
#endif
    // If it's the current active slot span, change it. We bounce the slot span
    // to the empty list as a force towards defragmentation.
    if (PA_LIKELY(this == bucket->active_slot_spans_head)) {
      bucket->SetNewActiveSlotSpan();
    }
    PA_DCHECK(bucket->active_slot_spans_head != this);

    if (CanStoreRawSize()) {
      SetRawSize(0);
    }

    RegisterEmpty();
  }
}

template <bool thread_safe>
void SlotSpanMetadata<thread_safe>::Decommit(PartitionRoot<thread_safe>* root) {
  root->lock_.AssertAcquired();
  PA_DCHECK(is_empty());
  PA_DCHECK(!bucket->is_direct_mapped());
  uintptr_t slot_span_start = SlotSpanMetadata::ToSlotSpanStart(this);
  // If lazy commit is enabled, only provisioned slots are committed.
  size_t dirty_size =
      base::bits::AlignUp(GetProvisionedSize(), SystemPageSize());
  size_t size_to_decommit =
      kUseLazyCommit ? dirty_size : bucket->get_bytes_per_span();

  PA_DCHECK(root->empty_slot_spans_dirty_bytes >= dirty_size);
  root->empty_slot_spans_dirty_bytes -= dirty_size;

  // Not decommitted slot span must've had at least 1 allocation.
  PA_DCHECK(size_to_decommit > 0);
  root->DecommitSystemPagesForData(
      slot_span_start, size_to_decommit,
      PageAccessibilityDisposition::kAllowKeepForPerf);

#if BUILDFLAG(USE_FREESLOT_BITMAP)
  FreeSlotBitmapReset(slot_span_start, slot_span_start + size_to_decommit,
                      bucket->slot_size);
#endif

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
  PA_DCHECK(in_empty_cache_);
  PA_DCHECK(empty_cache_index_ < kMaxFreeableSpans);
  PA_DCHECK(this == root->global_empty_slot_span_ring[empty_cache_index_]);
  in_empty_cache_ = 0;
  if (is_empty()) {
    Decommit(root);
  }
}

template <bool thread_safe>
void SlotSpanMetadata<thread_safe>::SortFreelist() {
  std::bitset<kMaxSlotsPerSlotSpan> free_slots;
  uintptr_t slot_span_start = ToSlotSpanStart(this);

  size_t num_provisioned_slots =
      bucket->get_slots_per_span() - num_unprovisioned_slots;
  PA_CHECK(num_provisioned_slots <= kMaxSlotsPerSlotSpan);

  size_t num_free_slots = 0;
  size_t slot_size = bucket->slot_size;
  for (PartitionFreelistEntry* head = freelist_head; head;
       head = head->GetNext(slot_size)) {
    ++num_free_slots;
    size_t offset_in_slot_span = SlotStartPtr2Addr(head) - slot_span_start;
    size_t slot_number = bucket->GetSlotNumber(offset_in_slot_span);
    PA_DCHECK(slot_number < num_provisioned_slots);
    free_slots[slot_number] = true;
  }
  PA_DCHECK(num_free_slots == GetFreelistLength());

  // Empty or single-element list is always sorted.
  if (num_free_slots > 1) {
    PartitionFreelistEntry* back = nullptr;
    PartitionFreelistEntry* head = nullptr;

    for (size_t slot_number = 0; slot_number < num_provisioned_slots;
         slot_number++) {
      if (free_slots[slot_number]) {
        uintptr_t slot_start = slot_span_start + (slot_size * slot_number);
        auto* entry = PartitionFreelistEntry::EmplaceAndInitNull(slot_start);

        if (!head) {
          head = entry;
        } else {
          back->SetNext(entry);
        }

        back = entry;
      }
    }
    SetFreelistHead(head);
  }

  freelist_is_sorted_ = true;
}

namespace {

void UnmapNow(uintptr_t reservation_start,
              size_t reservation_size,
              pool_handle pool) {
  PA_DCHECK(reservation_start && reservation_size > 0);
#if BUILDFLAG(PA_DCHECK_IS_ON)
  // When ENABLE_BACKUP_REF_PTR_SUPPORT is off, BRP pool isn't used.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (pool == kBRPPoolHandle) {
    // In 32-bit mode, the beginning of a reservation may be excluded from the
    // BRP pool, so shift the pointer. Other pools don't have this logic.
    PA_DCHECK(IsManagedByPartitionAllocBRPPool(
#if BUILDFLAG(HAS_64_BIT_POINTERS)
        reservation_start
#else
        reservation_start +
        AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap *
            AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap
#endif  // BUILDFLAG(HAS_64_BIT_POINTERS)
        ));
  } else
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  {
    PA_DCHECK(pool == kRegularPoolHandle
#if BUILDFLAG(ENABLE_PKEYS)
              || pool == kPkeyPoolHandle
#endif
#if BUILDFLAG(HAS_64_BIT_POINTERS)
              ||
              (IsConfigurablePoolAvailable() && pool == kConfigurablePoolHandle)
#endif
    );
    // Non-BRP pools don't need adjustment that BRP needs in 32-bit mode.
    PA_DCHECK(IsManagedByPartitionAllocRegularPool(reservation_start) ||
#if BUILDFLAG(ENABLE_PKEYS)
              IsManagedByPartitionAllocPkeyPool(reservation_start) ||
#endif
              IsManagedByPartitionAllocConfigurablePool(reservation_start));
  }
#endif  // BUILDFLAG(PA_DCHECK_IS_ON)

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

#if !BUILDFLAG(HAS_64_BIT_POINTERS)
  AddressPoolManager::GetInstance().MarkUnused(pool, reservation_start,
                                               reservation_size);
#endif

  // After resetting the table entries, unreserve and decommit the memory.
  AddressPoolManager::GetInstance().UnreserveAndDecommit(
      pool, reservation_start, reservation_size);
}

}  // namespace

template struct SlotSpanMetadata<ThreadSafe>;

}  // namespace partition_alloc::internal
