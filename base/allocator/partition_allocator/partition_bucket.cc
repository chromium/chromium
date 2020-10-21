// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_bucket.h"

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/object_bitmap.h"
#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_tag_bitmap.h"
#include "base/check.h"
#include "build/build_config.h"

namespace base {
namespace internal {

namespace {

template <bool thread_safe>
ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
PartitionDirectMap(PartitionRoot<thread_safe>* root, int flags, size_t raw_size)
    EXCLUSIVE_LOCKS_REQUIRED(root->lock_) {
  size_t size = PartitionBucket<thread_safe>::get_direct_map_size(raw_size);

  // Because we need to fake looking like a super page, we need to allocate
  // a bunch of system pages more than "size":
  // - The first few system pages are the partition page in which the super
  // page metadata is stored. We fault just one system page out of a partition
  // page sized clump.
  // - We add a trailing guard page on 32-bit (on 64-bit we rely on the
  // massive address space plus randomization instead).
  size_t map_size = size + PartitionPageSize();
#if !defined(ARCH_CPU_64_BITS)
  map_size += SystemPageSize();
#endif
  // Round up to the allocation granularity.
  map_size += PageAllocationGranularityOffsetMask();
  map_size &= PageAllocationGranularityBaseMask();

  char* ptr = nullptr;
  // Allocate from GigaCage, if enabled. However, the exception to this is when
  // tags aren't allowed, as CheckedPtr assumes that everything inside GigaCage
  // uses tags (specifically, inside the GigaCage's normal bucket pool).
  if (root->UsesGigaCage()) {
    ptr = internal::AddressPoolManager::GetInstance()->Alloc(GetDirectMapPool(),
                                                             nullptr, map_size);
  } else {
    ptr = reinterpret_cast<char*>(AllocPages(nullptr, map_size,
                                             kSuperPageAlignment, PageReadWrite,
                                             PageTag::kPartitionAlloc));
  }
  if (UNLIKELY(!ptr))
    return nullptr;

  size_t committed_page_size = size + SystemPageSize();
  root->total_size_of_direct_mapped_pages += committed_page_size;
  root->IncreaseCommittedPages(committed_page_size);

  char* slot = ptr + PartitionPageSize();
  SetSystemPagesAccess(ptr + (SystemPageSize() * 2),
                       PartitionPageSize() - (SystemPageSize() * 2),
                       PageInaccessible);
#if !defined(ARCH_CPU_64_BITS)
  SetSystemPagesAccess(ptr, SystemPageSize(), PageInaccessible);
  SetSystemPagesAccess(slot + size, SystemPageSize(), PageInaccessible);
#endif

  auto* metadata = reinterpret_cast<PartitionDirectMapMetadata<thread_safe>*>(
      PartitionSuperPageToMetadataArea(ptr));
  metadata->extent.root = root;
  // The new structures are all located inside a fresh system page so they
  // will all be zeroed out. These DCHECKs are for documentation.
  PA_DCHECK(!metadata->extent.super_page_base);
  PA_DCHECK(!metadata->extent.super_pages_end);
  PA_DCHECK(!metadata->extent.next);
  PA_DCHECK(PartitionPage<thread_safe>::FromPointerNoAlignmentCheck(slot) ==
            &metadata->page);

  auto* page = &metadata->page;
  PA_DCHECK(!page->slot_span_metadata_offset);
  PA_DCHECK(!page->slot_span_metadata.next_slot_span);
  PA_DCHECK(!page->slot_span_metadata.num_allocated_slots);
  PA_DCHECK(!page->slot_span_metadata.num_unprovisioned_slots);
  PA_DCHECK(!page->slot_span_metadata.empty_cache_index);
  page->slot_span_metadata.bucket = &metadata->bucket;
  page->slot_span_metadata.SetFreelistHead(
      reinterpret_cast<PartitionFreelistEntry*>(slot));

  auto* next_entry = reinterpret_cast<PartitionFreelistEntry*>(slot);
  next_entry->next = PartitionFreelistEntry::Encode(nullptr);

  PA_DCHECK(!metadata->bucket.active_slot_spans_head);
  PA_DCHECK(!metadata->bucket.empty_slot_spans_head);
  PA_DCHECK(!metadata->bucket.decommitted_slot_spans_head);
  PA_DCHECK(!metadata->bucket.num_system_pages_per_slot_span);
  PA_DCHECK(!metadata->bucket.num_full_slot_spans);
  metadata->bucket.slot_size = size;

  auto* map_extent = &metadata->direct_map_extent;
  map_extent->map_size = map_size - PartitionPageSize() - SystemPageSize();
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
ALWAYS_INLINE void* PartitionBucket<thread_safe>::AllocNewSlotSpan(
    PartitionRoot<thread_safe>* root,
    int flags,
    uint16_t num_partition_pages,
    size_t committed_size) {
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(root->next_partition_page) %
              PartitionPageSize()));
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(root->next_partition_page_end) %
              PartitionPageSize()));
  PA_DCHECK(num_partition_pages <= NumPartitionPagesPerSuperPage());
  PA_DCHECK(committed_size % SystemPageSize() == 0);
  size_t total_size = PartitionPageSize() * num_partition_pages;
  PA_DCHECK(committed_size <= total_size);
  size_t num_partition_pages_left =
      (root->next_partition_page_end - root->next_partition_page) >>
      PartitionPageShift();
  if (LIKELY(num_partition_pages_left >= num_partition_pages)) {
    // In this case, we can still hand out pages from the current super page
    // allocation.
    char* ret = root->next_partition_page;

    // Fresh System Pages in the SuperPages are decommited. Commit them
    // before vending them back.
    SetSystemPagesAccess(ret, committed_size, PageReadWrite);

    root->next_partition_page += total_size;
    root->IncreaseCommittedPages(committed_size);

#if ENABLE_TAG_FOR_MTE_CHECKED_PTR
    PA_DCHECK(root->next_tag_bitmap_page);
    char* next_tag_bitmap_page = reinterpret_cast<char*>(
        bits::Align(reinterpret_cast<uintptr_t>(
                        PartitionTagPointer(root->next_partition_page)),
                    SystemPageSize()));
    if (root->next_tag_bitmap_page < next_tag_bitmap_page) {
#if DCHECK_IS_ON()
      char* super_page = reinterpret_cast<char*>(
          reinterpret_cast<uintptr_t>(ret) & kSuperPageBaseMask);
      char* tag_bitmap = super_page + PartitionPageSize();
      PA_DCHECK(next_tag_bitmap_page <= tag_bitmap + ActualTagBitmapSize());
      PA_DCHECK(next_tag_bitmap_page > tag_bitmap);
#endif
      SetSystemPagesAccess(root->next_tag_bitmap_page,
                           next_tag_bitmap_page - root->next_tag_bitmap_page,
                           PageReadWrite);
      root->next_tag_bitmap_page = next_tag_bitmap_page;
    }
#if MTE_CHECKED_PTR_SET_TAG_AT_FREE
    // TODO(tasak): Consider initializing each slot with a different tag.
    PartitionTagSetValue(ret, total_size, root->GetNewPartitionTag());
#endif
#endif
    return ret;
  }

  // Need a new super page. We want to allocate super pages in a contiguous
  // address region as much as possible. This is important for not causing
  // page table bloat and not fragmenting address spaces in 32 bit
  // architectures.
  char* requested_address = root->next_super_page;
  char* super_page = nullptr;
  // Allocate from GigaCage, if enabled. However, the exception to this is when
  // tags aren't allowed, as CheckedPtr assumes that everything inside GigaCage
  // uses tags (specifically, inside the GigaCage's normal bucket pool).
  if (root->allow_extras && features::IsPartitionAllocGigaCageEnabled()) {
    super_page = AddressPoolManager::GetInstance()->Alloc(
        GetNormalBucketPool(), requested_address, kSuperPageSize);
  } else {
    super_page = reinterpret_cast<char*>(
        AllocPages(requested_address, kSuperPageSize, kSuperPageAlignment,
                   PageReadWrite, PageTag::kPartitionAlloc));
  }
  if (UNLIKELY(!super_page))
    return nullptr;

  root->total_size_of_super_pages += kSuperPageSize;

  // |total_size| MUST be less than kSuperPageSize - (PartitionPageSize()*2).
  // This is a trustworthy value because num_partition_pages is not user
  // controlled.
  //
  // TODO(ajwong): Introduce a DCHECK.
  root->next_super_page = super_page + kSuperPageSize;
  // TODO(tasak): Consider starting the bitmap right after metadata to save
  // space.
  char* tag_bitmap = super_page + PartitionPageSize();
  char* quarantine_bitmaps = tag_bitmap + ReservedTagBitmapSize();
  const size_t quarantine_bitmaps_size =
      root->scannable ? 2 * sizeof(QuarantineBitmap) : 0;
  PA_DCHECK(quarantine_bitmaps_size % PartitionPageSize() == 0);
  char* ret = quarantine_bitmaps + quarantine_bitmaps_size;
  root->next_partition_page = ret + total_size;
  root->next_partition_page_end = root->next_super_page - PartitionPageSize();
  PA_DCHECK(ret == SuperPagePayloadBegin(super_page, root->scannable));
  PA_DCHECK(root->next_partition_page_end == SuperPagePayloadEnd(super_page));

  // The first slot span is accessible. The given committed_size is equal to
  // the system-page-aligned size of the slot span.
  SetSystemPagesAccess(ret + committed_size,
                       (super_page + kSuperPageSize) - (ret + committed_size),
                       PageInaccessible);
  root->IncreaseCommittedPages(committed_size);

  // Make the first partition page in the super page a guard page, but leave a
  // hole in the middle.
  // This is where we put page metadata and also a tiny amount of extent
  // metadata.
  SetSystemPagesAccess(super_page, SystemPageSize(), PageInaccessible);
  SetSystemPagesAccess(super_page + (SystemPageSize() * 2),
                       PartitionPageSize() - (SystemPageSize() * 2),
                       PageInaccessible);
#if ENABLE_TAG_FOR_MTE_CHECKED_PTR
  // Make the first |total_size| region of the tag bitmap accessible.
  // The rest of the region is set to inaccessible.
  char* next_tag_bitmap_page = reinterpret_cast<char*>(
      bits::Align(reinterpret_cast<uintptr_t>(
                      PartitionTagPointer(root->next_partition_page)),
                  SystemPageSize()));
  PA_DCHECK(next_tag_bitmap_page <= tag_bitmap + ActualTagBitmapSize());
  PA_DCHECK(next_tag_bitmap_page > tag_bitmap);
  // |ret| points at the end of the tag bitmap.
  PA_DCHECK(next_tag_bitmap_page <= ret);
  SetSystemPagesAccess(next_tag_bitmap_page, ret - next_tag_bitmap_page,
                       PageInaccessible);
#if MTE_CHECKED_PTR_SET_TAG_AT_FREE
  // TODO(tasak): Consider initializing each slot with a different tag.
  PartitionTagSetValue(ret, total_size, root->GetNewPartitionTag());
#endif
  root->next_tag_bitmap_page = next_tag_bitmap_page;
#endif

  //  SetSystemPagesAccess(super_page + (kSuperPageSize -
  //  PartitionPageSize()),
  //                             PartitionPageSize(), PageInaccessible);
  // All remaining slotspans for the unallocated PartitionPages inside the
  // SuperPage are conceptually decommitted. Correctly set the state here
  // so they do not occupy resources.
  //
  // TODO(ajwong): Refactor Page Allocator API so the SuperPage comes in
  // decommited initially.
  SetSystemPagesAccess(
      super_page + PartitionPageSize() + ReservedTagBitmapSize() +
          quarantine_bitmaps_size + total_size,
      (kSuperPageSize - PartitionPageSize() - ReservedTagBitmapSize() -
       quarantine_bitmaps_size - total_size),
      PageInaccessible);

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
  // The bucket never changes. We set it up once.
  slot_span->bucket = this;
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
ALWAYS_INLINE char* PartitionBucket<thread_safe>::AllocAndFillFreelist(
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
      SlotSpanMetadata<thread_safe>::ToPointer(slot_span));
  char* return_object = base + (size * slot_span->num_allocated_slots);
  char* first_freelist_pointer = return_object + size;
  char* first_freelist_pointer_extent =
      first_freelist_pointer + sizeof(PartitionFreelistEntry*);
  // Our goal is to fault as few system pages as possible. We calculate the
  // page containing the "end" of the returned slot, and then allow freelist
  // pointers to be written up to the end of that page.
  char* sub_page_limit = reinterpret_cast<char*>(
      RoundUpToSystemPage(reinterpret_cast<size_t>(first_freelist_pointer)));
  char* slots_limit = return_object + (size * num_slots);
  char* freelist_limit = sub_page_limit;
  if (UNLIKELY(slots_limit < freelist_limit))
    freelist_limit = slots_limit;

  uint16_t num_new_freelist_entries = 0;
  if (LIKELY(first_freelist_pointer_extent <= freelist_limit)) {
    // Only consider used space in the slot span. If we consider wasted
    // space, we may get an off-by-one when a freelist pointer fits in the
    // wasted space, but a slot does not.
    // We know we can fit at least one freelist pointer.
    num_new_freelist_entries = 1;
    // Any further entries require space for the whole slot span.
    num_new_freelist_entries += static_cast<uint16_t>(
        (freelist_limit - first_freelist_pointer_extent) / size);
  }

  // We always return an object slot -- that's the +1 below.
  // We do not neccessarily create any new freelist entries, because we cross
  // sub page boundaries frequently for large bucket sizes.
  PA_DCHECK(num_new_freelist_entries + 1 <= num_slots);
  num_slots -= (num_new_freelist_entries + 1);
  slot_span->num_unprovisioned_slots = num_slots;
  slot_span->num_allocated_slots++;

  if (LIKELY(num_new_freelist_entries)) {
    char* freelist_pointer = first_freelist_pointer;
    auto* entry = reinterpret_cast<PartitionFreelistEntry*>(freelist_pointer);
    slot_span->SetFreelistHead(entry);
    while (--num_new_freelist_entries) {
      freelist_pointer += size;
      auto* next_entry =
          reinterpret_cast<PartitionFreelistEntry*>(freelist_pointer);
      entry->next = PartitionFreelistEntry::Encode(next_entry);
      entry = next_entry;
    }
    entry->next = PartitionFreelistEntry::Encode(nullptr);
  } else {
    slot_span->SetFreelistHead(nullptr);
  }
  return return_object;
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
      // tag it as full (via a negative value). We need it tagged so that
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
  bool return_null = flags & PartitionAllocReturnNull;
  if (UNLIKELY(is_direct_mapped())) {
    PA_DCHECK(raw_size > kMaxBucketed);
    PA_DCHECK(this == &root->sentinel_bucket);
    PA_DCHECK(active_slot_spans_head ==
              SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
    if (raw_size > MaxDirectMapped()) {
      if (return_null)
        return nullptr;
      // The lock is here to protect PA from:
      // 1. Concurrent calls
      // 2. Reentrant calls
      //
      // This is fine here however, as:
      // 1. Concurrency: |PartitionRoot::OutOfMemory()| never returns, so the
      //    lock will not be re-acquired, which would lead to acting on
      //    inconsistent data that could have been modified in-between releasing
      //    and acquiring it.
      // 2. Reentrancy: This is why we release the lock. On some platforms,
      //    terminating the process may free() memory, or even possibly try to
      //    allocate some. Calling free() is fine, but will deadlock since
      //    |PartitionRoot::lock_| is not recursive.
      //
      // Supporting reentrant calls properly is hard, and not a requirement for
      // PA. However up to that point, we've only *read* data, not *written* to
      // any state. Reentrant calls are then fine, especially as we don't
      // continue on this path. The only downside is possibly endless recursion
      // if the OOM handler allocates and fails to use UncheckedMalloc() or
      // equivalent, but that's violating the contract of
      // base::OnNoMemoryInternal().
      ScopedUnlockGuard<thread_safe> unlock{root->lock_};
      PartitionExcessiveAllocationSize(raw_size);
      IMMEDIATE_CRASH();  // Not required, kept as documentation.
    }
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
      new_slot_span = decommitted_slot_spans_head;
      PA_DCHECK(new_slot_span->bucket == this);
      PA_DCHECK(new_slot_span->is_decommitted());
      decommitted_slot_spans_head = new_slot_span->next_slot_span;
      void* addr = SlotSpanMetadata<thread_safe>::ToPointer(new_slot_span);
      root->RecommitSystemPages(addr,
                                new_slot_span->bucket->get_bytes_per_span());
      new_slot_span->Reset();
      *is_already_zeroed = kDecommittedPagesAreAlwaysZeroed;
    }
    PA_DCHECK(new_slot_span);
  } else {
    // Third. If we get here, we need a brand new slot span.
    uint16_t num_partition_pages = get_pages_per_slot_span();
    void* raw_memory = AllocNewSlotSpan(root, flags, num_partition_pages,
                                        get_bytes_per_span());
    if (LIKELY(raw_memory != nullptr)) {
      new_slot_span =
          SlotSpanMetadata<thread_safe>::FromPointerNoAlignmentCheck(
              raw_memory);
      InitializeSlotSpan(new_slot_span);
      // New memory from PageAllocator is always zeroed.
      *is_already_zeroed = true;
    }
  }

  // Bail if we had a memory allocation failure.
  if (UNLIKELY(!new_slot_span)) {
    PA_DCHECK(active_slot_spans_head ==
              SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
    if (return_null)
      return nullptr;
    // See comment above.
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
    PartitionFreelistEntry* new_head =
        EncodedPartitionFreelistEntry::Decode(entry->next);
    new_slot_span->SetFreelistHead(new_head);
    new_slot_span->num_allocated_slots++;
    return entry;
  }
  // Otherwise, we need to build the freelist.
  PA_DCHECK(new_slot_span->num_unprovisioned_slots);
  return AllocAndFillFreelist(new_slot_span);
}

template struct PartitionBucket<ThreadSafe>;
template struct PartitionBucket<NotThreadSafe>;

}  // namespace internal
}  // namespace base
