// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_bucket.h"

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/starscan/object_bitmap.h"
#include "base/bits.h"
#include "base/check.h"
#include "build/build_config.h"

namespace base {
namespace internal {

namespace {

template <bool thread_safe>
SlotSpanMetadata<thread_safe>*
PartitionDirectMap(PartitionRoot<thread_safe>* root, int flags, size_t raw_size)
    EXCLUSIVE_LOCKS_REQUIRED(root->lock_) {
  bool return_null = flags & PartitionAllocReturnNull;
  if (UNLIKELY(raw_size > MaxDirectMapped())) {
    if (return_null)
      return nullptr;

    // The lock is here to protect PA from:
    // 1. Concurrent calls
    // 2. Reentrant calls
    //
    // This is fine here however, as:
    // 1. Concurrency: |PartitionRoot::OutOfMemory()| never returns, so the lock
    //    will not be re-acquired, which would lead to acting on inconsistent
    //    data that could have been modified in-between releasing and acquiring
    //    it.
    // 2. Reentrancy: This is why we release the lock. On some platforms,
    //    terminating the process may free() memory, or even possibly try to
    //    allocate some. Calling free() is fine, but will deadlock since
    //    |PartitionRoot::lock_| is not recursive.
    //
    // Supporting reentrant calls properly is hard, and not a requirement for
    // PA. However up to that point, we've only *read* data, not *written* to
    // any state. Reentrant calls are then fine, especially as we don't continue
    // on this path. The only downside is possibly endless recursion if the OOM
    // handler allocates and fails to use UncheckedMalloc() or equivalent, but
    // that's violating the contract of base::TerminateBecauseOutOfMemory().
    ScopedUnlockGuard<thread_safe> unlock{root->lock_};
    PartitionExcessiveAllocationSize(raw_size);
    IMMEDIATE_CRASH();  // Not required, kept as documentation.
  }

  size_t slot_size = PartitionRoot<thread_safe>::GetDirectMapSlotSize(raw_size);
  size_t reserved_size = root->GetDirectMapReservedSize(raw_size);
  size_t map_size =
      reserved_size -
      PartitionRoot<thread_safe>::GetDirectMapMetadataAndGuardPagesSize();
  PA_DCHECK(slot_size <= map_size);

  char* ptr = nullptr;
  // Allocate from GigaCage, if enabled. In this case, use non-BRP pool, because
  // BackupRefPtr isn't supported in direct maps.
  bool with_giga_cage = features::IsPartitionAllocGigaCageEnabled();
  if (with_giga_cage) {
    ptr = internal::AddressPoolManager::GetInstance()->Reserve(
        GetNonBRPPool(), nullptr, reserved_size);
  } else {
    ptr = reinterpret_cast<char*>(
        AllocPages(nullptr, reserved_size, kSuperPageAlignment,
                   PageInaccessible, PageTag::kPartitionAlloc));
  }
  if (UNLIKELY(!ptr)) {
    if (return_null)
      return nullptr;

    // Crash handling is split on purpose in this function:
    // - Crashing here likely means that Chrome is out of address space (on 32
    //   bit platforms), or out of GigaCage space (on 64 bit ones).
    // - Crashing below would likely mean out of commit charge.
    //
    // See comment above regarding unlocking,
    ScopedUnlockGuard<thread_safe> unlock{root->lock_};
    root->OutOfMemory(raw_size);
    IMMEDIATE_CRASH();  // Not required, kept as documentation.
  }

  root->total_size_of_direct_mapped_pages.fetch_add(reserved_size,
                                                    std::memory_order_relaxed);

  char* slot = ptr + PartitionPageSize();
  RecommitSystemPages(ptr + SystemPageSize(), SystemPageSize(), PageReadWrite,
                      PageUpdatePermissions);
  // It is typically possible to map a large range of inaccessible pages, and
  // this is leveraged in multiple places, including the GigaCage. However, this
  // doesn't mean that we can commit all this memory.  For the vast majority of
  // allocations, this just means that we crash in a slightly different places,
  // but for callers ready to handle failures, we have to return nullptr.
  // See crbug.com/1187404.
  //
  // Note that we didn't check above, because if we cannot even commit a single
  // page, then this is likely hopeless anyway, and we will crash very soon.
  bool ok = root->TryRecommitSystemPagesForData(slot, slot_size,
                                                PageUpdatePermissions);
  if (!ok) {
    if (with_giga_cage) {
      internal::AddressPoolManager::GetInstance()->UnreserveAndDecommit(
          GetNonBRPPool(), ptr, reserved_size);
    } else {
      FreePages(ptr, reserved_size);
    }

    if (return_null)
      return nullptr;

    // See comment above.
    ScopedUnlockGuard<thread_safe> unlock{root->lock_};
    root->OutOfMemory(raw_size);
    IMMEDIATE_CRASH();  // Not required, kept as documentation.
  }

  auto* metadata = reinterpret_cast<PartitionDirectMapMetadata<thread_safe>*>(
      PartitionSuperPageToMetadataArea(ptr));
  metadata->extent.root = root;
  // The new structures are all located inside a fresh system page so they
  // will all be zeroed out. These DCHECKs are for documentation.
  PA_DCHECK(!metadata->extent.super_page_base);
  PA_DCHECK(!metadata->extent.super_pages_end);
  PA_DCHECK(!metadata->extent.next);
  // Call FromSlotInnerPtr instead of FromSlotStartPtr, because the bucket isn't
  // set up yet to properly assert the slot start.
  PA_DCHECK(PartitionPage<thread_safe>::FromSlotInnerPtr(slot) ==
            &metadata->page);

  auto* page = &metadata->page;
  PA_DCHECK(!page->slot_span_metadata_offset);
  PA_DCHECK(!page->slot_span_metadata.next_slot_span);
  PA_DCHECK(!page->slot_span_metadata.num_allocated_slots);
  PA_DCHECK(!page->slot_span_metadata.num_unprovisioned_slots);
  PA_DCHECK(!page->slot_span_metadata.empty_cache_index);

  PA_DCHECK(!metadata->bucket.active_slot_spans_head);
  PA_DCHECK(!metadata->bucket.empty_slot_spans_head);
  PA_DCHECK(!metadata->bucket.decommitted_slot_spans_head);
  PA_DCHECK(!metadata->bucket.num_system_pages_per_slot_span);
  PA_DCHECK(!metadata->bucket.num_full_slot_spans);
  metadata->bucket.slot_size = slot_size;

  new (&page->slot_span_metadata)
      SlotSpanMetadata<thread_safe>(&metadata->bucket);
  auto* next_entry = new (slot) PartitionFreelistEntry();
  page->slot_span_metadata.SetFreelistHead(next_entry);

  auto* map_extent = &metadata->direct_map_extent;
  map_extent->map_size = map_size;
  map_extent->bucket = &metadata->bucket;

  // Maintain the doubly-linked list of all direct mappings.
  map_extent->next_extent = root->direct_map_list;
  if (map_extent->next_extent)
    map_extent->next_extent->prev_extent = map_extent;
  map_extent->prev_extent = nullptr;
  root->direct_map_list = map_extent;

  return &page->slot_span_metadata;
}

}  // namespace

// TODO(ajwong): This seems to interact badly with
// get_pages_per_slot_span() which rounds the value from this up to a
// multiple of NumSystemPagesPerPartitionPage() (aka 4) anyways.
// http://crbug.com/776537
//
// TODO(ajwong): The waste calculation seems wrong. The PTE usage should cover
// both used and unsed pages.
// http://crbug.com/776537
template <bool thread_safe>
uint8_t PartitionBucket<thread_safe>::get_system_pages_per_slot_span() {
  // This works out reasonably for the current bucket sizes of the generic
  // allocator, and the current values of partition page size and constants.
  // Specifically, we have enough room to always pack the slots perfectly into
  // some number of system pages. The only waste is the waste associated with
  // unfaulted pages (i.e. wasted address space).
  // TODO: we end up using a lot of system pages for very small sizes. For
  // example, we'll use 12 system pages for slot size 24. The slot size is
  // so small that the waste would be tiny with just 4, or 1, system pages.
  // Later, we can investigate whether there are anti-fragmentation benefits
  // to using fewer system pages.
  double best_waste_ratio = 1.0f;
  uint16_t best_pages = 0;
  if (slot_size > MaxSystemPagesPerSlotSpan() * SystemPageSize()) {
    // TODO(ajwong): Why is there a DCHECK here for this?
    // http://crbug.com/776537
    PA_DCHECK(!(slot_size % SystemPageSize()));
    best_pages = static_cast<uint16_t>(slot_size / SystemPageSize());
    // TODO(ajwong): Should this be checking against
    // MaxSystemPagesPerSlotSpan() or numeric_limits<uint8_t>::max?
    // http://crbug.com/776537
    PA_CHECK(best_pages < (1 << 8));
    return static_cast<uint8_t>(best_pages);
  }
  PA_DCHECK(slot_size <= MaxSystemPagesPerSlotSpan() * SystemPageSize());
  for (uint16_t i = NumSystemPagesPerPartitionPage() - 1;
       i <= MaxSystemPagesPerSlotSpan(); ++i) {
    size_t page_size = SystemPageSize() * i;
    size_t num_slots = page_size / slot_size;
    size_t waste = page_size - (num_slots * slot_size);
    // Leaving a page unfaulted is not free; the page will occupy an empty page
    // table entry.  Make a simple attempt to account for that.
    //
    // TODO(ajwong): This looks wrong. PTEs are allocated for all pages
    // regardless of whether or not they are wasted. Should it just
    // be waste += i * sizeof(void*)?
    // http://crbug.com/776537
    size_t num_remainder_pages = i & (NumSystemPagesPerPartitionPage() - 1);
    size_t num_unfaulted_pages =
        num_remainder_pages
            ? (NumSystemPagesPerPartitionPage() - num_remainder_pages)
            : 0;
    waste += sizeof(void*) * num_unfaulted_pages;
    double waste_ratio =
        static_cast<double>(waste) / static_cast<double>(page_size);
    if (waste_ratio < best_waste_ratio) {
      best_waste_ratio = waste_ratio;
      best_pages = i;
    }
  }
  PA_DCHECK(best_pages > 0);
  PA_CHECK(best_pages <= MaxSystemPagesPerSlotSpan());
  return static_cast<uint8_t>(best_pages);
}

template <bool thread_safe>
void PartitionBucket<thread_safe>::Init(uint32_t new_slot_size) {
  slot_size = new_slot_size;
  slot_size_reciprocal = kReciprocalMask / new_slot_size + 1;
  active_slot_spans_head =
      SlotSpanMetadata<thread_safe>::get_sentinel_slot_span();
  empty_slot_spans_head = nullptr;
  decommitted_slot_spans_head = nullptr;
  num_full_slot_spans = 0;
  num_system_pages_per_slot_span = get_system_pages_per_slot_span();
}

template <bool thread_safe>
NOINLINE void PartitionBucket<thread_safe>::OnFull() {
  OOM_CRASH(0);
}

template <bool thread_safe>
ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
PartitionBucket<thread_safe>::AllocNewSlotSpan(PartitionRoot<thread_safe>* root,
                                               int flags) {
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(root->next_partition_page) %
              PartitionPageSize()));
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(root->next_partition_page_end) %
              PartitionPageSize()));

  size_t num_partition_pages = get_pages_per_slot_span();
  size_t slot_span_reserved_size = PartitionPageSize() * num_partition_pages;
  size_t slot_span_committed_size = get_bytes_per_span();
  PA_DCHECK(num_partition_pages <= NumPartitionPagesPerSuperPage());
  PA_DCHECK(slot_span_committed_size % SystemPageSize() == 0);
  PA_DCHECK(slot_span_committed_size <= slot_span_reserved_size);

  size_t num_partition_pages_left =
      (root->next_partition_page_end - root->next_partition_page) >>
      PartitionPageShift();
  if (UNLIKELY(num_partition_pages_left < num_partition_pages)) {
    // In this case, we can no longer hand out pages from the current super page
    // allocation. Get a new super page.
    if (!AllocNewSuperPage(root)) {
      return nullptr;
    }
  }

  void* slot_span_start = root->next_partition_page;
  root->next_partition_page += slot_span_reserved_size;

  // Call FromSlotInnerPtr instead of FromSlotStartPtr, because the slot_span's
  // bucket isn't set up yet to properly assert the slot start.
  auto* slot_span =
      SlotSpanMetadata<thread_safe>::FromSlotInnerPtr(slot_span_start);
  InitializeSlotSpan(slot_span);

  // System pages in the super page come in a decommited state. Commit them
  // before vending them back.
  // If lazy commit is enabled, pages will be committed when provisioning slots,
  // in ProvisionMoreSlotsAndAllocOne(), not here.
  if (!root->use_lazy_commit) {
    root->RecommitSystemPagesForData(slot_span_start, slot_span_committed_size,
                                     PageUpdatePermissions);
  }

  // Double check that we had enough space in the super page for the new slot
  // span.
  PA_DCHECK(root->next_partition_page <= root->next_partition_page_end);
  return slot_span;
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionBucket<thread_safe>::AllocNewSuperPage(
    PartitionRoot<thread_safe>* root) {
  // Need a new super page. We want to allocate super pages in a contiguous
  // address region as much as possible. This is important for not causing
  // page table bloat and not fragmenting address spaces in 32 bit
  // architectures.
  char* requested_address = root->next_super_page;
  char* super_page = nullptr;
  // Allocate from GigaCage, if enabled. Route to the appropriate GigaCage pool
  // based on BackupRefPtr support.
  if (features::IsPartitionAllocGigaCageEnabled()) {
    super_page = AddressPoolManager::GetInstance()->Reserve(
        root->UseBRPPool() ? GetBRPPool() : GetNonBRPPool(), requested_address,
        kSuperPageSize);

#if !defined(PA_HAS_64_BITS_POINTERS) && BUILDFLAG(USE_BRP_POOL_BLOCKLIST)
    if (root->UseBRPPool()) {
      constexpr int kMaxRandomAddressTries = 10;
      for (int i = 0; i < kMaxRandomAddressTries; ++i) {
        if (!super_page ||
            AddressPoolManagerBitmap::IsAllowedSuperPageForBRPPool(super_page))
          break;
        AddressPoolManager::GetInstance()->UnreserveAndDecommit(
            GetBRPPool(), super_page, kSuperPageSize);
        super_page = AddressPoolManager::GetInstance()->Reserve(
            GetBRPPool(), nullptr, kSuperPageSize);
      }

      // If the allocation attempt succeeds, we will break out of the following
      // loop immediately.
      //
      // Last resort: sequentially scan the whole 32-bit address space. The
      // number of blocked super-pages should be very small, so we expect to
      // practically never need to run the following code. Note that it may fail
      // to find an available page, e.g., when it becomes available after the
      // scan passes through it, but we accept the risk.
      for (uintptr_t ptr = kSuperPageSize; ptr != 0; ptr += kSuperPageSize) {
        if (!super_page ||
            AddressPoolManagerBitmap::IsAllowedSuperPageForBRPPool(super_page))
          break;
        AddressPoolManager::GetInstance()->UnreserveAndDecommit(
            GetBRPPool(), super_page, kSuperPageSize);
        super_page = AddressPoolManager::GetInstance()->Reserve(
            GetBRPPool(), reinterpret_cast<void*>(ptr), kSuperPageSize);
      }

      if (super_page &&
          !AddressPoolManagerBitmap::IsAllowedSuperPageForBRPPool(super_page)) {
        AddressPoolManager::GetInstance()->UnreserveAndDecommit(
            GetBRPPool(), super_page, kSuperPageSize);
        super_page = nullptr;
      }
    }
#endif
  } else {
    super_page = reinterpret_cast<char*>(
        AllocPages(requested_address, kSuperPageSize, kSuperPageAlignment,
                   PageInaccessible, PageTag::kPartitionAlloc));
  }
  if (UNLIKELY(!super_page))
    return nullptr;

  root->total_size_of_super_pages.fetch_add(kSuperPageSize,
                                            std::memory_order_relaxed);

  root->next_super_page = super_page + kSuperPageSize;
  char* quarantine_bitmaps = super_page + PartitionPageSize();
  const size_t quarantine_bitmaps_reserved_size =
      root->IsQuarantineAllowed() ? ReservedQuarantineBitmapsSize() : 0;
  const size_t quarantine_bitmaps_size_to_commit =
      root->IsQuarantineAllowed() ? CommittedQuarantineBitmapsSize() : 0;
  PA_DCHECK(quarantine_bitmaps_reserved_size % PartitionPageSize() == 0);
  PA_DCHECK(quarantine_bitmaps_size_to_commit % SystemPageSize() == 0);
  PA_DCHECK(quarantine_bitmaps_size_to_commit <=
            quarantine_bitmaps_reserved_size);
  char* ret = quarantine_bitmaps + quarantine_bitmaps_reserved_size;
  root->next_partition_page = ret;
  root->next_partition_page_end = root->next_super_page - PartitionPageSize();
  PA_DCHECK(ret ==
            SuperPagePayloadBegin(super_page, root->IsQuarantineAllowed()));
  PA_DCHECK(root->next_partition_page_end == SuperPagePayloadEnd(super_page));

  // Keep the first partition page in the super page inaccessible to serve as a
  // guard page, except an "island" in the middle where we put page metadata and
  // also a tiny amount of extent metadata.
  RecommitSystemPages(super_page + SystemPageSize(),
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
                      // Allocate 2 SystemPages, one for SuperPage metadata and
                      // the other for RefCount bitmap.
                      SystemPageSize() * 2,
#else
                      SystemPageSize(),
#endif
                      PageReadWrite, PageUpdatePermissions);

  // If PCScan is used, commit the quarantine bitmap. Otherwise, leave it
  // uncommitted and let PartitionRoot::EnablePCScan commit it when needed.
  if (root->IsQuarantineEnabled()) {
    RecommitSystemPages(quarantine_bitmaps, quarantine_bitmaps_size_to_commit,
                        PageReadWrite, PageUpdatePermissions);
  }

  // If we were after a specific address, but didn't get it, assume that
  // the system chose a lousy address. Here most OS'es have a default
  // algorithm that isn't randomized. For example, most Linux
  // distributions will allocate the mapping directly before the last
  // successful mapping, which is far from random. So we just get fresh
  // randomness for the next mapping attempt.
  if (requested_address && requested_address != super_page)
    root->next_super_page = nullptr;

  // We allocated a new super page so update super page metadata.
  // First check if this is a new extent or not.
  auto* latest_extent =
      reinterpret_cast<PartitionSuperPageExtentEntry<thread_safe>*>(
          PartitionSuperPageToMetadataArea(super_page));
  // By storing the root in every extent metadata object, we have a fast way
  // to go from a pointer within the partition to the root object.
  latest_extent->root = root;
  // Most new extents will be part of a larger extent, and these three fields
  // are unused, but we initialize them to 0 so that we get a clear signal
  // in case they are accidentally used.
  latest_extent->super_page_base = nullptr;
  latest_extent->super_pages_end = nullptr;
  latest_extent->next = nullptr;

  PartitionSuperPageExtentEntry<thread_safe>* current_extent =
      root->current_extent;
  const bool is_new_extent = super_page != requested_address;
  if (UNLIKELY(is_new_extent)) {
    if (UNLIKELY(!current_extent)) {
      PA_DCHECK(!root->first_extent);
      root->first_extent = latest_extent;
    } else {
      PA_DCHECK(current_extent->super_page_base);
      current_extent->next = latest_extent;
    }
    root->current_extent = latest_extent;
    latest_extent->super_page_base = super_page;
    latest_extent->super_pages_end = super_page + kSuperPageSize;
  } else {
    // We allocated next to an existing extent so just nudge the size up a
    // little.
    PA_DCHECK(current_extent->super_pages_end);
    current_extent->super_pages_end += kSuperPageSize;
    PA_DCHECK(ret >= current_extent->super_page_base &&
              ret < current_extent->super_pages_end);
  }
  return ret;
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionBucket<thread_safe>::InitializeSlotSpan(
    SlotSpanMetadata<thread_safe>* slot_span) {
  new (slot_span) SlotSpanMetadata<thread_safe>(this);
  slot_span->empty_cache_index = -1;

  slot_span->Reset();

  uint16_t num_partition_pages = get_pages_per_slot_span();
  auto* page = reinterpret_cast<PartitionPage<thread_safe>*>(slot_span);
  for (uint16_t i = 1; i < num_partition_pages; ++i) {
    auto* secondary_page = page + i;
    secondary_page->slot_span_metadata_offset = i;
  }
}

template <bool thread_safe>
ALWAYS_INLINE char* PartitionBucket<thread_safe>::ProvisionMoreSlotsAndAllocOne(
    PartitionRoot<thread_safe>* root,
    SlotSpanMetadata<thread_safe>* slot_span) {
  PA_DCHECK(slot_span !=
            SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
  uint16_t num_slots = slot_span->num_unprovisioned_slots;
  PA_DCHECK(num_slots);
  // We should only get here when _every_ slot is either used or unprovisioned.
  // (The third state is "on the freelist". If we have a non-empty freelist, we
  // should not get here.)
  PA_DCHECK(num_slots + slot_span->num_allocated_slots == get_slots_per_span());
  // Similarly, make explicitly sure that the freelist is empty.
  PA_DCHECK(!slot_span->freelist_head);
  PA_DCHECK(slot_span->num_allocated_slots >= 0);

  size_t size = slot_size;
  char* base = reinterpret_cast<char*>(
      SlotSpanMetadata<thread_safe>::ToSlotSpanStartPtr(slot_span));
  // If we got here, the first unallocated slot is either partially or fully on
  // an uncommitted page. If the latter, it must be at the start of that page.
  char* return_slot = base + (size * slot_span->num_allocated_slots);
  char* next_slot = return_slot + size;
  char* commit_start = bits::AlignUp(return_slot, SystemPageSize());
  PA_DCHECK(next_slot > commit_start);
  char* commit_end = bits::AlignUp(next_slot, SystemPageSize());
  // If the slot was partially committed, |return_slot| and |next_slot| fall
  // in different pages. If the slot was fully uncommitted, |return_slot| points
  // to the page start and |next_slot| doesn't, thus only the latter gets
  // rounded up.
  PA_DCHECK(commit_end > commit_start);

  // The slot being returned is considered allocated.
  slot_span->num_allocated_slots++;
  // Round down, because a slot that doesn't fully fit in the new page(s) isn't
  // provisioned.
  uint16_t slots_to_provision = (commit_end - return_slot) / size;
  slot_span->num_unprovisioned_slots -= slots_to_provision;
  PA_DCHECK(slot_span->num_allocated_slots +
                slot_span->num_unprovisioned_slots <=
            get_slots_per_span());

  // If lazy commit is enabled, meaning system pages in the slot span come
  // in an initially decommitted state, commit them here.
  // Note, we can't use PageKeepPermissionsIfPossible, because we have no
  // knowledge which pages have been committed before (it doesn't matter on
  // Windows anyway).
  if (root->use_lazy_commit) {
    // TODO(lizeb): Handle commit failure.
    root->RecommitSystemPagesForData(commit_start, commit_end - commit_start,
                                     PageUpdatePermissions);
  }

  // Add all slots that fit within so far committed pages to the free list.
  PartitionFreelistEntry* prev_entry = nullptr;
  char* next_slot_end = next_slot + size;
  size_t free_list_entries_added = 0;
  while (next_slot_end <= commit_end) {
    auto* entry = new (next_slot) PartitionFreelistEntry();
    if (!slot_span->freelist_head) {
      PA_DCHECK(!prev_entry);
      PA_DCHECK(!free_list_entries_added);
      slot_span->SetFreelistHead(entry);
    } else {
      PA_DCHECK(free_list_entries_added);
      prev_entry->SetNext(entry);
    }
    next_slot = next_slot_end;
    next_slot_end = next_slot + size;
    prev_entry = entry;
#if DCHECK_IS_ON()
    free_list_entries_added++;
#endif
  }

#if DCHECK_IS_ON()
  // The only provisioned slot not added to the free list is the one being
  // returned.
  PA_DCHECK(slots_to_provision == free_list_entries_added + 1);
  // We didn't necessarily provision more than one slot (e.g. if |slot_size|
  // is large), meaning that |slot_span->freelist_head| can be nullptr.
  if (slot_span->freelist_head) {
    PA_DCHECK(free_list_entries_added);
    slot_span->freelist_head->CheckFreeList();
  }
#endif

  return return_slot;
}

template <bool thread_safe>
bool PartitionBucket<thread_safe>::SetNewActiveSlotSpan() {
  SlotSpanMetadata<thread_safe>* slot_span = active_slot_spans_head;
  if (slot_span == SlotSpanMetadata<thread_safe>::get_sentinel_slot_span())
    return false;

  SlotSpanMetadata<thread_safe>* next_slot_span;

  for (; slot_span; slot_span = next_slot_span) {
    next_slot_span = slot_span->next_slot_span;
    PA_DCHECK(slot_span->bucket == this);
    PA_DCHECK(slot_span != empty_slot_spans_head);
    PA_DCHECK(slot_span != decommitted_slot_spans_head);

    if (LIKELY(slot_span->is_active())) {
      // This slot span is usable because it has freelist entries, or has
      // unprovisioned slots we can create freelist entries from.
      active_slot_spans_head = slot_span;
      return true;
    }

    // Deal with empty and decommitted slot spans.
    if (LIKELY(slot_span->is_empty())) {
      slot_span->next_slot_span = empty_slot_spans_head;
      empty_slot_spans_head = slot_span;
    } else if (LIKELY(slot_span->is_decommitted())) {
      slot_span->next_slot_span = decommitted_slot_spans_head;
      decommitted_slot_spans_head = slot_span;
    } else {
      PA_DCHECK(slot_span->is_full());
      // If we get here, we found a full slot span. Skip over it too, and also
      // mark it as full (via a negative value). We need it marked so that
      // free'ing can tell, and move it back into the active list.
      slot_span->num_allocated_slots = -slot_span->num_allocated_slots;
      ++num_full_slot_spans;
      // num_full_slot_spans is a uint16_t for efficient packing so guard
      // against overflow to be safe.
      if (UNLIKELY(!num_full_slot_spans))
        OnFull();
      // Not necessary but might help stop accidents.
      slot_span->next_slot_span = nullptr;
    }
  }

  active_slot_spans_head =
      SlotSpanMetadata<thread_safe>::get_sentinel_slot_span();
  return false;
}

template <bool thread_safe>
void* PartitionBucket<thread_safe>::SlowPathAlloc(
    PartitionRoot<thread_safe>* root,
    int flags,
    size_t raw_size,
    bool* is_already_zeroed) {
  // The slow path is called when the freelist is empty.
  PA_DCHECK(!active_slot_spans_head->freelist_head);

  SlotSpanMetadata<thread_safe>* new_slot_span = nullptr;
  // |new_slot_span->bucket| will always be |this|, except when |this| is the
  // sentinel bucket, which is used to signal a direct mapped allocation.  In
  // this case |new_bucket| will be set properly later. This avoids a read for
  // most allocations.
  PartitionBucket* new_bucket = this;
  *is_already_zeroed = false;

  // For the PartitionRoot::Alloc() API, we have a bunch of buckets
  // marked as special cases. We bounce them through to the slow path so that
  // we can still have a blazing fast hot path due to lack of corner-case
  // branches.
  //
  // Note: The ordering of the conditionals matter! In particular,
  // SetNewActiveSlotSpan() has a side-effect even when returning
  // false where it sweeps the active list and may move things into the empty or
  // decommitted lists which affects the subsequent conditional.
  if (UNLIKELY(is_direct_mapped())) {
    PA_DCHECK(raw_size > kMaxBucketed);
    PA_DCHECK(this == &root->sentinel_bucket);
    PA_DCHECK(active_slot_spans_head ==
              SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());

    // No fast path for direct-mapped allocations.
    if (flags & PartitionAllocFastPathOrReturnNull)
      return nullptr;

    new_slot_span = PartitionDirectMap(root, flags, raw_size);
    if (new_slot_span)
      new_bucket = new_slot_span->bucket;
    // Memory from PageAllocator is always zeroed.
    *is_already_zeroed = true;
  } else if (LIKELY(SetNewActiveSlotSpan())) {
    // First, did we find an active slot span in the active list?
    new_slot_span = active_slot_spans_head;
    PA_DCHECK(new_slot_span->is_active());
  } else if (LIKELY(empty_slot_spans_head != nullptr) ||
             LIKELY(decommitted_slot_spans_head != nullptr)) {
    // Second, look in our lists of empty and decommitted slot spans.
    // Check empty slot spans first, which are preferred, but beware that an
    // empty slot span might have been decommitted.
    while (LIKELY((new_slot_span = empty_slot_spans_head) != nullptr)) {
      PA_DCHECK(new_slot_span->bucket == this);
      PA_DCHECK(new_slot_span->is_empty() || new_slot_span->is_decommitted());
      empty_slot_spans_head = new_slot_span->next_slot_span;
      // Accept the empty slot span unless it got decommitted.
      if (new_slot_span->freelist_head) {
        new_slot_span->next_slot_span = nullptr;
        break;
      }
      PA_DCHECK(new_slot_span->is_decommitted());
      new_slot_span->next_slot_span = decommitted_slot_spans_head;
      decommitted_slot_spans_head = new_slot_span;
    }
    if (UNLIKELY(!new_slot_span) &&
        LIKELY(decommitted_slot_spans_head != nullptr)) {
      // Commit can be expensive, don't do it.
      if (flags & PartitionAllocFastPathOrReturnNull)
        return nullptr;

      new_slot_span = decommitted_slot_spans_head;
      PA_DCHECK(new_slot_span->bucket == this);
      PA_DCHECK(new_slot_span->is_decommitted());
      decommitted_slot_spans_head = new_slot_span->next_slot_span;

      // If lazy commit is enabled, pages will be recommitted when provisioning
      // slots, in ProvisionMoreSlotsAndAllocOne(), not here.
      if (!root->use_lazy_commit) {
        void* addr =
            SlotSpanMetadata<thread_safe>::ToSlotSpanStartPtr(new_slot_span);
        // If lazy commit was never used, we have a guarantee that all slot span
        // pages have been previously committed, and then decommitted using
        // PageKeepPermissionsIfPossible, so use the same option as an
        // optimization. Otherwise fall back to PageUpdatePermissions (slower).
        // (Insider knowledge: as of writing this comment, lazy commit is only
        // used on Windows and this flag is ignored there, thus no perf impact.)
        // TODO(lizeb): Handle commit failure.
        root->RecommitSystemPagesForData(
            addr, new_slot_span->bucket->get_bytes_per_span(),
            root->never_used_lazy_commit ? PageKeepPermissionsIfPossible
                                         : PageUpdatePermissions);
      }

      new_slot_span->Reset();
      *is_already_zeroed = kDecommittedPagesAreAlwaysZeroed;
    }
    PA_DCHECK(new_slot_span);
  } else {
    // Getting a new slot span is expensive, don't do it.
    if (flags & PartitionAllocFastPathOrReturnNull)
      return nullptr;

    // Third. If we get here, we need a brand new slot span.
    // TODO(bartekn): For single-slot slot spans, we can use rounded raw_size
    // as slot_span_committed_size.
    new_slot_span = AllocNewSlotSpan(root, flags);
    // New memory from PageAllocator is always zeroed.
    *is_already_zeroed = true;
  }

  // Bail if we had a memory allocation failure.
  if (UNLIKELY(!new_slot_span)) {
    PA_DCHECK(active_slot_spans_head ==
              SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
    if (flags & PartitionAllocReturnNull)
      return nullptr;
    // See comment in PartitionDirectMap() for unlocking.
    ScopedUnlockGuard<thread_safe> unlock{root->lock_};
    root->OutOfMemory(raw_size);
    IMMEDIATE_CRASH();  // Not required, kept as documentation.
  }

  PA_DCHECK(new_bucket != &root->sentinel_bucket);
  new_bucket->active_slot_spans_head = new_slot_span;
  if (new_slot_span->CanStoreRawSize())
    new_slot_span->SetRawSize(raw_size);

  // If we found an active slot span with free slots, or an empty slot span, we
  // have a usable freelist head.
  if (LIKELY(new_slot_span->freelist_head != nullptr)) {
    PartitionFreelistEntry* entry = new_slot_span->freelist_head;
    PartitionFreelistEntry* new_head = entry->GetNext();
    new_slot_span->SetFreelistHead(new_head);
    new_slot_span->num_allocated_slots++;

    // We likely set *is_already_zeroed to true above, make sure that the
    // freelist entry doesn't contain data.
    return entry->ClearForAllocation();
  }

  // Otherwise, we need to provision more slots by committing more pages. Build
  // the free list for the newly provisioned slots.
  PA_DCHECK(new_slot_span->num_unprovisioned_slots);
  return ProvisionMoreSlotsAndAllocOne(root, new_slot_span);
}

template struct PartitionBucket<ThreadSafe>;
template struct PartitionBucket<NotThreadSafe>;

}  // namespace internal
}  // namespace base
