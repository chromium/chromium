// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_

#include <string.h>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/partition_tag.h"
#include "base/allocator/partition_allocator/random.h"
#include "base/check_op.h"
#include "base/thread_annotations.h"

namespace base {
namespace internal {

// An "extent" is a span of consecutive superpages. We link the partition's next
// extent (if there is one) to the very start of a superpage's metadata area.
template <bool thread_safe>
struct PartitionSuperPageExtentEntry {
  PartitionRoot<thread_safe>* root;
  char* super_page_base;
  char* super_pages_end;
  PartitionSuperPageExtentEntry<thread_safe>* next;
};
static_assert(
    sizeof(PartitionSuperPageExtentEntry<ThreadSafe>) <= kPageMetadataSize,
    "PartitionSuperPageExtentEntry must be able to fit in a metadata slot");

// PartitionPage::Free() defers unmapping a large page until the lock is
// released. Callers of PartitionPage::Free() must invoke Run().
// TODO(1061437): Reconsider once the new locking mechanism is implemented.
struct DeferredUnmap {
  void* ptr = nullptr;
  size_t size = 0;

  // In most cases there is no page to unmap and ptr == nullptr. This function
  // is inlined to avoid the overhead of a function call in the common case.
  ALWAYS_INLINE void Run();

 private:
  BASE_EXPORT NOINLINE void Unmap();
};

// Some notes on page states. A page can be in one of four major states:
// 1) Active.
// 2) Full.
// 3) Empty.
// 4) Decommitted.
// An active page has available free slots. A full page has no free slots. An
// empty page has no free slots, and a decommitted page is an empty page that
// had its backing memory released back to the system.
// There are two linked lists tracking the pages. The "active page" list is an
// approximation of a list of active pages. It is an approximation because
// full, empty and decommitted pages may briefly be present in the list until
// we next do a scan over it.
// The "empty page" list is an accurate list of pages which are either empty
// or decommitted.
//
// The significant page transitions are:
// - free() will detect when a full page has a slot free()'d and immediately
// return the page to the head of the active list.
// - free() will detect when a page is fully emptied. It _may_ add it to the
// empty list or it _may_ leave it on the active list until a future list scan.
// - malloc() _may_ scan the active page list in order to fulfil the request.
// If it does this, full, empty and decommitted pages encountered will be
// booted out of the active list. If there are no suitable active pages found,
// an empty or decommitted page (if one exists) will be pulled from the empty
// list on to the active list.
//
// TODO(ajwong): Evaluate if this should be named PartitionSlotSpanMetadata or
// similar. If so, all uses of the term "page" in comments, member variables,
// local variables, and documentation that refer to this concept should be
// updated.
template <bool thread_safe>
struct PartitionPage {
  union {
    struct {
      PartitionFreelistEntry* freelist_head;
      PartitionPage<thread_safe>* next_page;
      PartitionBucket<thread_safe>* bucket;
      // Deliberately signed, 0 for empty or decommitted page, -n for full
      // pages:
      int16_t num_allocated_slots;
      uint16_t num_unprovisioned_slots;
      uint16_t page_offset;
      int16_t empty_cache_index;  // -1 if not in the empty cache.
    };

    // sizeof(PartitionPage) must always be:
    // - a power of 2 (for fast modulo operations)
    // - below kPageMetadataSize
    //
    // This makes sure that this is respected no matter the architecture.
    char optional_padding[kPageMetadataSize];
  };
  // Public API
  // Note the matching Alloc() functions are in PartitionPage.
  // Callers must invoke DeferredUnmap::Run() after releasing the lock.
  BASE_EXPORT NOINLINE DeferredUnmap FreeSlowPath() WARN_UNUSED_RESULT;
  ALWAYS_INLINE DeferredUnmap Free(void* ptr) WARN_UNUSED_RESULT;

  void Decommit(PartitionRoot<thread_safe>* root);
  void DecommitIfPossible(PartitionRoot<thread_safe>* root);

  // Pointer manipulation functions. These must be static as the input |page|
  // pointer may be the result of an offset calculation and therefore cannot
  // be trusted. The objective of these functions is to sanitize this input.
  ALWAYS_INLINE static void* ToPointer(const PartitionPage* page);
  ALWAYS_INLINE static PartitionPage* FromPointerNoAlignmentCheck(void* ptr);
  ALWAYS_INLINE static PartitionPage* FromPointer(void* ptr);

  // Returns either the exact allocated size for direct-mapped and single-slot
  // buckets, or the slot size. The second one is an overestimate of the real
  // allocated size.
  ALWAYS_INLINE size_t GetAllocatedSize() const {
    // Allocated size can be:
    // - The slot size for small enough buckets.
    // - Stored exactly, for large buckets (see get_raw_size_ptr()), and
    //   direct-mapped allocations.
    size_t result = bucket->slot_size;
    if (UNLIKELY(get_raw_size_ptr()))  // has row size.
      result = get_raw_size();

    return result;
  }

  ALWAYS_INLINE const size_t* get_raw_size_ptr() const;
  ALWAYS_INLINE size_t* get_raw_size_ptr() {
    return const_cast<size_t*>(
        const_cast<const PartitionPage*>(this)->get_raw_size_ptr());
  }

  ALWAYS_INLINE size_t get_raw_size() const;
  ALWAYS_INLINE void set_raw_size(size_t size);

  ALWAYS_INLINE void Reset();

  // TODO(ajwong): Can this be made private?  https://crbug.com/787153
  BASE_EXPORT static PartitionPage* get_sentinel_page();

  // Page State accessors.
  // Note that it's only valid to call these functions on pages found on one of
  // the page lists. Specifically, you can't call these functions on full pages
  // that were detached from the active list.
  //
  // This restriction provides the flexibity for some of the status fields to
  // be repurposed when a page is taken off a list. See the negation of
  // |num_allocated_slots| when a full page is removed from the active list
  // for an example of such repurposing.
  ALWAYS_INLINE bool is_active() const;
  ALWAYS_INLINE bool is_full() const;
  ALWAYS_INLINE bool is_empty() const;
  ALWAYS_INLINE bool is_decommitted() const;

 private:
  // g_sentinel_page is used as a sentinel to indicate that there is no page
  // in the active page list. We can use nullptr, but in that case we need
  // to add a null-check branch to the hot allocation path. We want to avoid
  // that.
  //
  // Note, this declaration is kept in the header as opposed to an anonymous
  // namespace so the getter can be fully inlined.
  static PartitionPage sentinel_page_;
};
static_assert(sizeof(PartitionPage<ThreadSafe>) == kPageMetadataSize,
              "PartitionPage must be able to fit in a metadata slot");

ALWAYS_INLINE char* PartitionSuperPageToMetadataArea(char* ptr) {
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(ptr);
  PA_DCHECK(!(pointer_as_uint & kSuperPageOffsetMask));
  // The metadata area is exactly one system page (the guard page) into the
  // super page.
  return reinterpret_cast<char*>(pointer_as_uint + kSystemPageSize);
}

// See the comment for |FromPointer|.
template <bool thread_safe>
ALWAYS_INLINE PartitionPage<thread_safe>*
PartitionPage<thread_safe>::FromPointerNoAlignmentCheck(void* ptr) {
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(ptr);
  char* super_page_ptr =
      reinterpret_cast<char*>(pointer_as_uint & kSuperPageBaseMask);
  uintptr_t partition_page_index =
      (pointer_as_uint & kSuperPageOffsetMask) >> kPartitionPageShift;
  // Index 0 is invalid because it is the super page extent metadata and the
  // last index is invalid because the whole PartitionPage is set as guard
  // pages.
  PA_DCHECK(partition_page_index);
  PA_DCHECK(partition_page_index < kNumPartitionPagesPerSuperPage - 1);
  auto* page = reinterpret_cast<PartitionPage*>(
      PartitionSuperPageToMetadataArea(super_page_ptr) +
      (partition_page_index << kPageMetadataShift));
  // Partition pages in the same slot span share the same page object. Adjust
  // for that.
  size_t delta = page->page_offset << kPageMetadataShift;
  page =
      reinterpret_cast<PartitionPage*>(reinterpret_cast<char*>(page) - delta);
  return page;
}

// Converts from a pointer to the PartitionPage object (within super pages's
// metadata) into a pointer to the beginning of the partition page.
// This doesn't have to be the first page in the slot span.
template <bool thread_safe>
ALWAYS_INLINE void* PartitionPage<thread_safe>::ToPointer(
    const PartitionPage<thread_safe>* page) {
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(page);

  uintptr_t super_page_offset = (pointer_as_uint & kSuperPageOffsetMask);

  // A valid |page| must be past the first guard System page and within
  // the following metadata region.
  PA_DCHECK(super_page_offset > kSystemPageSize);
  // Must be less than total metadata region.
  PA_DCHECK(super_page_offset <
            kSystemPageSize +
                (kNumPartitionPagesPerSuperPage * kPageMetadataSize));
  uintptr_t partition_page_index =
      (super_page_offset - kSystemPageSize) >> kPageMetadataShift;
  // Index 0 is invalid because it is the super page extent metadata and the
  // last index is invalid because the whole PartitionPage is set as guard
  // pages.
  PA_DCHECK(partition_page_index);
  PA_DCHECK(partition_page_index < kNumPartitionPagesPerSuperPage - 1);
  uintptr_t super_page_base = (pointer_as_uint & kSuperPageBaseMask);
  void* ret = reinterpret_cast<void*>(
      super_page_base + (partition_page_index << kPartitionPageShift));
  return ret;
}

// Converts from a pointer inside a partition page into a pointer to the
// PartitionPage object (within super pages's metadata).
// The first PartitionPage of the slot span will be returned, regardless where
// inside of the slot span |ptr| points to.
template <bool thread_safe>
ALWAYS_INLINE PartitionPage<thread_safe>*
PartitionPage<thread_safe>::FromPointer(void* ptr) {
  PartitionPage* page = PartitionPage::FromPointerNoAlignmentCheck(ptr);
  // Checks that the pointer is a multiple of bucket size.
  PA_DCHECK(!((reinterpret_cast<uintptr_t>(ptr) -
               reinterpret_cast<uintptr_t>(PartitionPage::ToPointer(page))) %
              page->bucket->slot_size));
  return page;
}

template <bool thread_safe>
ALWAYS_INLINE const size_t* PartitionPage<thread_safe>::get_raw_size_ptr()
    const {
  // For single-slot buckets which span more than
  // |kMaxPartitionPagesPerSlotSpan| partition pages, we have some spare
  // metadata space to store the raw allocation size. We can use this to report
  // better statistics.
  if (LIKELY(bucket->slot_size <= kMaxSystemPagesPerSlotSpan * kSystemPageSize))
    return nullptr;

  PA_DCHECK((bucket->slot_size % kSystemPageSize) == 0);
  PA_DCHECK(bucket->is_direct_mapped() || bucket->get_slots_per_span() == 1);

  const PartitionPage* the_next_page = this + 1;
  return reinterpret_cast<const size_t*>(&the_next_page->freelist_head);
}

template <bool thread_safe>
ALWAYS_INLINE size_t PartitionPage<thread_safe>::get_raw_size() const {
  const size_t* ptr = get_raw_size_ptr();
  if (UNLIKELY(ptr != nullptr))
    return *ptr;
  return 0;
}

template <bool thread_safe>
ALWAYS_INLINE DeferredUnmap PartitionPage<thread_safe>::Free(void* ptr) {
#if DCHECK_IS_ON()
  auto* root = PartitionRoot<thread_safe>::FromPage(this);
  root->lock_.AssertAcquired();
#endif

  PA_DCHECK(num_allocated_slots);
  // Catches an immediate double free.
  PA_CHECK(ptr != freelist_head);
  // Look for double free one level deeper in debug.
  PA_DCHECK(!freelist_head ||
            ptr != EncodedPartitionFreelistEntry::Decode(freelist_head->next));
  auto* entry = static_cast<internal::PartitionFreelistEntry*>(ptr);
  entry->next = internal::PartitionFreelistEntry::Encode(freelist_head);
  freelist_head = entry;
  --num_allocated_slots;
  if (UNLIKELY(num_allocated_slots <= 0)) {
    return FreeSlowPath();
  } else {
    // All single-slot allocations must go through the slow path to
    // correctly update the size metadata.
    PA_DCHECK(get_raw_size() == 0);
  }
  return {};
}

template <bool thread_safe>
ALWAYS_INLINE bool PartitionPage<thread_safe>::is_active() const {
  PA_DCHECK(this != get_sentinel_page());
  PA_DCHECK(!page_offset);
  return (num_allocated_slots > 0 &&
          (freelist_head || num_unprovisioned_slots));
}

template <bool thread_safe>
ALWAYS_INLINE bool PartitionPage<thread_safe>::is_full() const {
  PA_DCHECK(this != get_sentinel_page());
  PA_DCHECK(!page_offset);
  bool ret = (num_allocated_slots == bucket->get_slots_per_span());
  if (ret) {
    PA_DCHECK(!freelist_head);
    PA_DCHECK(!num_unprovisioned_slots);
  }
  return ret;
}

template <bool thread_safe>
ALWAYS_INLINE bool PartitionPage<thread_safe>::is_empty() const {
  PA_DCHECK(this != get_sentinel_page());
  PA_DCHECK(!page_offset);
  return (!num_allocated_slots && freelist_head);
}

template <bool thread_safe>
ALWAYS_INLINE bool PartitionPage<thread_safe>::is_decommitted() const {
  PA_DCHECK(this != get_sentinel_page());
  PA_DCHECK(!page_offset);
  bool ret = (!num_allocated_slots && !freelist_head);
  if (ret) {
    PA_DCHECK(!num_unprovisioned_slots);
    PA_DCHECK(empty_cache_index == -1);
  }
  return ret;
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionPage<thread_safe>::set_raw_size(size_t size) {
  size_t* raw_size_ptr = get_raw_size_ptr();
  if (UNLIKELY(raw_size_ptr != nullptr))
    *raw_size_ptr = size;
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionPage<thread_safe>::Reset() {
  PA_DCHECK(is_decommitted());

  num_unprovisioned_slots = bucket->get_slots_per_span();
  PA_DCHECK(num_unprovisioned_slots);

  next_page = nullptr;
}

ALWAYS_INLINE void DeferredUnmap::Run() {
  if (UNLIKELY(ptr)) {
    Unmap();
  }
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_
