// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_bucket.h"

#include "base/allocator/partition_allocator/address_pool_manager.h"
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
ALWAYS_INLINE PartitionPage<thread_safe>*
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
  if (root->allow_extras && IsPartitionAllocGigaCageEnabled()) {
#if defined(PA_HAS_64_BITS_POINTERS)
    ptr = internal::AddressPoolManager::GetInstance()->Alloc(GetDirectMapPool(),
                                                             map_size);
#else
    NOTREACHED();
#endif  // defined(PA_HAS_64_BITS_POINTERS)
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
  PA_DCHECK(!page->next_page);
  PA_DCHECK(!page->num_allocated_slots);
  PA_DCHECK(!page->num_unprovisioned_slots);
  PA_DCHECK(!page->page_offset);
  PA_DCHECK(!page->empty_cache_index);
  page->bucket = &metadata->bucket;
  page->freelist_head = reinterpret_cast<PartitionFreelistEntry*>(slot);

  auto* next_entry = reinterpret_cast<PartitionFreelistEntry*>(slot);
  next_entry->next = PartitionFreelistEntry::Encode(nullptr);

  PA_DCHECK(!metadata->bucket.active_pages_head);
  PA_DCHECK(!metadata->bucket.empty_pages_head);
  PA_DCHECK(!metadata->bucket.decommitted_pages_head);
  PA_DCHECK(!metadata->bucket.num_system_pages_per_slot_span);
  PA_DCHECK(!metadata->bucket.num_full_pages);
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

  return page;
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
  active_pages_head = PartitionPage<thread_safe>::get_sentinel_page();
  empty_pages_head = nullptr;
  decommitted_pages_head = nullptr;
  num_full_pages = 0;
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
    uint16_t num_partition_pages) {
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(root->next_partition_page) %
              PartitionPageSize()));
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(root->next_partition_page_end) %
              PartitionPageSize()));
  PA_DCHECK(num_partition_pages <= NumPartitionPagesPerSuperPage());
  size_t total_size = PartitionPageSize() * num_partition_pages;
  size_t num_partition_pages_left =
      (root->next_partition_page_end - root->next_partition_page) >>
      PartitionPageShift();
  if (LIKELY(num_partition_pages_left >= num_partition_pages)) {
    // In this case, we can still hand out pages from the current super page
    // allocation.
    char* ret = root->next_partition_page;

    // Fresh System Pages in the SuperPages are decommited. Commit them
    // before vending them back.
    SetSystemPagesAccess(ret, total_size, PageReadWrite);

    root->next_partition_page += total_size;
    root->IncreaseCommittedPages(total_size);

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
      PA_DCHECK(next_tag_bitmap_page <= tag_bitmap + kActualTagBitmapSize);
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

  // Need a new super page. We want to allocate super pages in a continguous
  // address region as much as possible. This is important for not causing
  // page table bloat and not fragmenting address spaces in 32 bit
  // architectures.
  char* requested_address = root->next_super_page;
  char* super_page = nullptr;
  // Allocate from GigaCage, if enabled. However, the exception to this is when
  // tags aren't allowed, as CheckedPtr assumes that everything inside GigaCage
  // uses tags (specifically, inside the GigaCage's normal bucket pool).
  if (root->allow_extras && IsPartitionAllocGigaCageEnabled()) {
#if defined(PA_HAS_64_BITS_POINTERS)
    super_page = AddressPoolManager::GetInstance()->Alloc(GetNormalBucketPool(),
                                                          kSuperPageSize);
#else
    NOTREACHED();
#endif
  } else {
    super_page = reinterpret_cast<char*>(
        AllocPages(requested_address, kSuperPageSize, kSuperPageAlignment,
                   PageReadWrite, PageTag::kPartitionAlloc));
  }
  if (UNLIKELY(!super_page))
    return nullptr;

  root->total_size_of_super_pages += kSuperPageSize;
  root->IncreaseCommittedPages(total_size);

  // |total_size| MUST be less than kSuperPageSize - (PartitionPageSize()*2).
  // This is a trustworthy value because num_partition_pages is not user
  // controlled.
  //
  // TODO(ajwong): Introduce a DCHECK.
  root->next_super_page = super_page + kSuperPageSize;
  // TODO(tasak): Consider starting the bitmap right after metadata to save
  // space.
  char* tag_bitmap = super_page + PartitionPageSize();
  char* ret = tag_bitmap + kReservedTagBitmapSize;
  root->next_partition_page = ret + total_size;
  root->next_partition_page_end = root->next_super_page - PartitionPageSize();
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
  PA_DCHECK(next_tag_bitmap_page <= tag_bitmap + kActualTagBitmapSize);
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
      super_page + PartitionPageSize() + kReservedTagBitmapSize + total_size,
      (kSuperPageSize - PartitionPageSize() - kReservedTagBitmapSize -
       total_size),
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
ALWAYS_INLINE uint16_t PartitionBucket<thread_safe>::get_pages_per_slot_span() {
  // Rounds up to nearest multiple of NumSystemPagesPerPartitionPage().
  return (num_system_pages_per_slot_span +
          (NumSystemPagesPerPartitionPage() - 1)) /
         NumSystemPagesPerPartitionPage();
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionBucket<thread_safe>::InitializeSlotSpan(
    PartitionPage<thread_safe>* page) {
  // The bucket never changes. We set it up once.
  page->bucket = this;
  page->empty_cache_index = -1;

  page->Reset();

  uint16_t num_partition_pages = get_pages_per_slot_span();
  char* page_char_ptr = reinterpret_cast<char*>(page);
  for (uint16_t i = 1; i < num_partition_pages; ++i) {
    page_char_ptr += kPageMetadataSize;
    auto* secondary_page =
        reinterpret_cast<PartitionPage<thread_safe>*>(page_char_ptr);
    secondary_page->page_offset = i;
  }
}

template <bool thread_safe>
ALWAYS_INLINE char* PartitionBucket<thread_safe>::AllocAndFillFreelist(
    PartitionPage<thread_safe>* page) {
  PA_DCHECK(page != PartitionPage<thread_safe>::get_sentinel_page());
  uint16_t num_slots = page->num_unprovisioned_slots;
  PA_DCHECK(num_slots);
  // We should only get here when _every_ slot is either used or unprovisioned.
  // (The third state is "on the freelist". If we have a non-empty freelist, we
  // should not get here.)
  PA_DCHECK(num_slots + page->num_allocated_slots == get_slots_per_span());
  // Similarly, make explicitly sure that the freelist is empty.
  PA_DCHECK(!page->freelist_head);
  PA_DCHECK(page->num_allocated_slots >= 0);

  size_t size = slot_size;
  char* base =
      reinterpret_cast<char*>(PartitionPage<thread_safe>::ToPointer(page));
  char* return_object = base + (size * page->num_allocated_slots);
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
  page->num_unprovisioned_slots = num_slots;
  page->num_allocated_slots++;

  if (LIKELY(num_new_freelist_entries)) {
    char* freelist_pointer = first_freelist_pointer;
    auto* entry = reinterpret_cast<PartitionFreelistEntry*>(freelist_pointer);
    page->freelist_head = entry;
    while (--num_new_freelist_entries) {
      freelist_pointer += size;
      auto* next_entry =
          reinterpret_cast<PartitionFreelistEntry*>(freelist_pointer);
      entry->next = PartitionFreelistEntry::Encode(next_entry);
      entry = next_entry;
    }
    entry->next = PartitionFreelistEntry::Encode(nullptr);
  } else {
    page->freelist_head = nullptr;
  }
  return return_object;
}

template <bool thread_safe>
bool PartitionBucket<thread_safe>::SetNewActivePage() {
  PartitionPage<thread_safe>* page = active_pages_head;
  if (page == PartitionPage<thread_safe>::get_sentinel_page())
    return false;

  PartitionPage<thread_safe>* next_page;

  for (; page; page = next_page) {
    next_page = page->next_page;
    PA_DCHECK(page->bucket == this);
    PA_DCHECK(page != empty_pages_head);
    PA_DCHECK(page != decommitted_pages_head);

    if (LIKELY(page->is_active())) {
      // This page is usable because it has freelist entries, or has
      // unprovisioned slots we can create freelist entries from.
      active_pages_head = page;
      return true;
    }

    // Deal with empty and decommitted pages.
    if (LIKELY(page->is_empty())) {
      page->next_page = empty_pages_head;
      empty_pages_head = page;
    } else if (LIKELY(page->is_decommitted())) {
      page->next_page = decommitted_pages_head;
      decommitted_pages_head = page;
    } else {
      PA_DCHECK(page->is_full());
      // If we get here, we found a full page. Skip over it too, and also
      // tag it as full (via a negative value). We need it tagged so that
      // free'ing can tell, and move it back into the active page list.
      page->num_allocated_slots = -page->num_allocated_slots;
      ++num_full_pages;
      // num_full_pages is a uint16_t for efficient packing so guard against
      // overflow to be safe.
      if (UNLIKELY(!num_full_pages))
        OnFull();
      // Not necessary but might help stop accidents.
      page->next_page = nullptr;
    }
  }

  active_pages_head = PartitionPage<thread_safe>::get_sentinel_page();
  return false;
}

template <bool thread_safe>
void* PartitionBucket<thread_safe>::SlowPathAlloc(
    PartitionRoot<thread_safe>* root,
    int flags,
    size_t size,
    bool* is_already_zeroed) {
  // The slow path is called when the freelist is empty.
  PA_DCHECK(!active_pages_head->freelist_head);

  PartitionPage<thread_safe>* new_page = nullptr;
  // |new_page->bucket| will always be |this|, except when |this| is the
  // sentinel bucket, which is used to signal a direct mapped allocation.  In
  // this case |new_page_bucket| will be set properly later. This avoids a read
  // for most allocations.
  PartitionBucket* new_page_bucket = this;
  *is_already_zeroed = false;

  // For the PartitionRoot::Alloc() API, we have a bunch of buckets
  // marked as special cases. We bounce them through to the slow path so that
  // we can still have a blazing fast hot path due to lack of corner-case
  // branches.
  //
  // Note: The ordering of the conditionals matter! In particular,
  // SetNewActivePage() has a side-effect even when returning
  // false where it sweeps the active page list and may move things into
  // the empty or decommitted lists which affects the subsequent conditional.
  bool return_null = flags & PartitionAllocReturnNull;
  if (UNLIKELY(is_direct_mapped())) {
    PA_DCHECK(size > kMaxBucketed);
    PA_DCHECK(this == &root->sentinel_bucket);
    PA_DCHECK(active_pages_head ==
              PartitionPage<thread_safe>::get_sentinel_page());
    if (size > MaxDirectMapped()) {
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
      PartitionExcessiveAllocationSize(size);
      IMMEDIATE_CRASH();  // Not required, kept as documentation.
    }
    new_page = PartitionDirectMap(root, flags, size);
    if (new_page)
      new_page_bucket = new_page->bucket;
    // New pages from PageAllocator are always zeroed.
    *is_already_zeroed = true;
  } else if (LIKELY(SetNewActivePage())) {
    // First, did we find an active page in the active pages list?
    new_page = active_pages_head;
    PA_DCHECK(new_page->is_active());
  } else if (LIKELY(empty_pages_head != nullptr) ||
             LIKELY(decommitted_pages_head != nullptr)) {
    // Second, look in our lists of empty and decommitted pages.
    // Check empty pages first, which are preferred, but beware that an
    // empty page might have been decommitted.
    while (LIKELY((new_page = empty_pages_head) != nullptr)) {
      PA_DCHECK(new_page->bucket == this);
      PA_DCHECK(new_page->is_empty() || new_page->is_decommitted());
      empty_pages_head = new_page->next_page;
      // Accept the empty page unless it got decommitted.
      if (new_page->freelist_head) {
        new_page->next_page = nullptr;
        break;
      }
      PA_DCHECK(new_page->is_decommitted());
      new_page->next_page = decommitted_pages_head;
      decommitted_pages_head = new_page;
    }
    if (UNLIKELY(!new_page) && LIKELY(decommitted_pages_head != nullptr)) {
      new_page = decommitted_pages_head;
      PA_DCHECK(new_page->bucket == this);
      PA_DCHECK(new_page->is_decommitted());
      decommitted_pages_head = new_page->next_page;
      void* addr = PartitionPage<thread_safe>::ToPointer(new_page);
      root->RecommitSystemPages(addr, new_page->bucket->get_bytes_per_span());
      new_page->Reset();
      *is_already_zeroed = kDecommittedPagesAreAlwaysZeroed;
    }
    PA_DCHECK(new_page);
  } else {
    // Third. If we get here, we need a brand new page.
    uint16_t num_partition_pages = get_pages_per_slot_span();
    void* raw_pages = AllocNewSlotSpan(root, flags, num_partition_pages);
    if (LIKELY(raw_pages != nullptr)) {
      new_page =
          PartitionPage<thread_safe>::FromPointerNoAlignmentCheck(raw_pages);
      InitializeSlotSpan(new_page);
      // New pages from PageAllocator are always zeroed.
      *is_already_zeroed = true;
    }
  }

  // Bail if we had a memory allocation failure.
  if (UNLIKELY(!new_page)) {
    PA_DCHECK(active_pages_head ==
              PartitionPage<thread_safe>::get_sentinel_page());
    if (return_null)
      return nullptr;
    // See comment above.
    ScopedUnlockGuard<thread_safe> unlock{root->lock_};
    root->OutOfMemory(size);
    IMMEDIATE_CRASH();  // Not required, kept as documentation.
  }

  PA_DCHECK(new_page_bucket != &root->sentinel_bucket);
  new_page_bucket->active_pages_head = new_page;
  new_page->set_raw_size(size);

  // If we found an active page with free slots, or an empty page, we have a
  // usable freelist head.
  if (LIKELY(new_page->freelist_head != nullptr)) {
    PartitionFreelistEntry* entry = new_page->freelist_head;
    PartitionFreelistEntry* new_head =
        EncodedPartitionFreelistEntry::Decode(entry->next);
    new_page->freelist_head = new_head;
    new_page->num_allocated_slots++;
    return entry;
  }
  // Otherwise, we need to build the freelist.
  PA_DCHECK(new_page->num_unprovisioned_slots);
  return AllocAndFillFreelist(new_page);
}

template struct PartitionBucket<ThreadSafe>;
template struct PartitionBucket<NotThreadSafe>;

}  // namespace internal
}  // namespace base
