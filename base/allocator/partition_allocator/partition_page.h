// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_

#include <string.h>
#include <cstdint>
#include <limits>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/starscan/object_bitmap.h"
#include "base/base_export.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/thread_annotations.h"

#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
#include "base/allocator/partition_allocator/partition_ref_count.h"
#endif

namespace base {
namespace internal {

// An "extent" is a span of consecutive superpages. We link the partition's next
// extent (if there is one) to the very start of a superpage's metadata area.
template <bool thread_safe>
struct PartitionSuperPageExtentEntry {
  PartitionRoot<thread_safe>* root;
  PartitionSuperPageExtentEntry<thread_safe>* next;
  uint16_t number_of_consecutive_super_pages;
  uint16_t number_of_nonempty_slot_spans;

  ALWAYS_INLINE void IncrementNumberOfNonemptySlotSpans();
  ALWAYS_INLINE void DecrementNumberOfNonemptySlotSpans();
};
static_assert(
    sizeof(PartitionSuperPageExtentEntry<ThreadSafe>) <= kPageMetadataSize,
    "PartitionSuperPageExtentEntry must be able to fit in a metadata slot");
static_assert(
    kMaxSuperPages / kSuperPageSize <=
        std::numeric_limits<
            decltype(PartitionSuperPageExtentEntry<
                     ThreadSafe>::number_of_consecutive_super_pages)>::max(),
    "number_of_consecutive_super_pages must be big enough");

// Returns the base of the first super page in the range of consecutive super
// pages. CAUTION! |extent| must point to the extent of the first super page in
// the range of consecutive super pages.
template <bool thread_safe>
ALWAYS_INLINE char* SuperPagesBeginFromExtent(
    PartitionSuperPageExtentEntry<thread_safe>* extent) {
  PA_DCHECK(0 < extent->number_of_consecutive_super_pages);
  PA_DCHECK(IsManagedByNormalBuckets(extent));
  return base::bits::AlignDown(reinterpret_cast<char*>(extent),
                               kSuperPageAlignment);
}

// Returns the end of the last super page in the range of consecutive super
// pages. CAUTION! |extent| must point to the extent of the first super page in
// the range of consecutive super pages.
template <bool thread_safe>
ALWAYS_INLINE char* SuperPagesEndFromExtent(
    PartitionSuperPageExtentEntry<thread_safe>* extent) {
  return SuperPagesBeginFromExtent(extent) +
         (extent->number_of_consecutive_super_pages * kSuperPageSize);
}

// SlotSpanMetadata::Free() defers unmapping a large page until the lock is
// released. Callers of SlotSpanMetadata::Free() must invoke Run().
// TODO(1061437): Reconsider once the new locking mechanism is implemented.
struct DeferredUnmap {
  void* reservation_start = nullptr;
  size_t reservation_size = 0;
  pool_handle giga_cage_pool;

  // In most cases there is no page to unmap and reservation_start == nullptr.
  // This function is inlined to avoid the overhead of a function call in the
  // common case.
  ALWAYS_INLINE void Run();

 private:
  BASE_EXPORT NOINLINE void Unmap();
};

using QuarantineBitmap =
    ObjectBitmap<kSuperPageSize, kSuperPageAlignment, kAlignment>;

// Metadata of the slot span.
//
// Some notes on slot span states. It can be in one of four major states:
// 1) Active.
// 2) Full.
// 3) Empty.
// 4) Decommitted.
// An active slot span has available free slots, as well as allocated ones.
// A full slot span has no free slots. An empty slot span has no allocated
// slots, and a decommitted slot span is an empty one that had its backing
// memory released back to the system.
//
// There are three linked lists tracking slot spans. The "active" list is an
// approximation of a list of active slot spans. It is an approximation because
// full, empty and decommitted slot spans may briefly be present in the list
// until we next do a scan over it. The "empty" list holds mostly empty slot
// spans, but may briefly hold decommitted ones too. The "decommitted" list
// holds only decommitted slot spans.
//
// The significant slot span transitions are:
// - Free() will detect when a full slot span has a slot freed and immediately
//   return the slot span to the head of the active list.
// - Free() will detect when a slot span is fully emptied. It _may_ add it to
//   the empty list or it _may_ leave it on the active list until a future
//   list scan.
// - Alloc() _may_ scan the active page list in order to fulfil the request.
//   If it does this, full, empty and decommitted slot spans encountered will be
//   booted out of the active list. If there are no suitable active slot spans
//   found, an empty or decommitted slot spans (if one exists) will be pulled
//   from the empty/decommitted list on to the active list.
template <bool thread_safe>
struct __attribute__((packed)) SlotSpanMetadata {
  PartitionFreelistEntry* freelist_head = nullptr;
  SlotSpanMetadata<thread_safe>* next_slot_span = nullptr;
  PartitionBucket<thread_safe>* const bucket;

  // Deliberately signed, 0 for empty or decommitted slot spans, -n for full
  // slot spans:
  int16_t num_allocated_slots = 0;
  uint16_t num_unprovisioned_slots = 0;
  int8_t empty_cache_index = 0;  // -1 if not in the empty cache.
                                 // < kMaxFreeableSpans.
  static_assert(kMaxFreeableSpans < std::numeric_limits<int8_t>::max(), "");

  constexpr static size_t kSystemPagesInSmallRegularSpanBits = 5;
  constexpr static size_t kMaxSystemPagesPerRegularSlotSpan =
      kMaxPartitionPagesPerRegularSlotSpan * 4;
  static_assert(kMaxSystemPagesPerRegularSlotSpan <
                    (1 << kSystemPagesInSmallRegularSpanBits),
                "");
  uint8_t can_store_raw_size : 1;

 private:
  // For small buckets buckets (i.e. when can_store_raw_size is false), stores
  // value 0-16, as there may be at most 4 partition pages in a slot span,
  // each having 4 system pages. (Note, we might be able to save a bit if
  // needed, because 0 occurs only upon slot allocation and can likley be
  // special-cased).
  // Otherwise, store only 0 or 1, as there can be only one slot in a span and
  // it's easy to calculate its size in pages.
  // Don't access this field directly, use GetPreviouslyCommittedSize() and
  // IncrasePreviouslyCommittedSize() instead.
  uint8_t num_previously_committed_system_pages
      : kSystemPagesInSmallRegularSpanBits;
  uint8_t unused : 2;

 public:
  explicit SlotSpanMetadata(PartitionBucket<thread_safe>* bucket);

  // Public API
  // Note the matching Alloc() functions are in PartitionPage.
  // Callers must invoke DeferredUnmap::Run() after releasing the lock.
  BASE_EXPORT NOINLINE DeferredUnmap FreeSlowPath() WARN_UNUSED_RESULT;
  ALWAYS_INLINE DeferredUnmap Free(void* ptr) WARN_UNUSED_RESULT;

  void Decommit(PartitionRoot<thread_safe>* root);
  void DecommitIfPossible(PartitionRoot<thread_safe>* root);

  // Pointer manipulation functions. These must be static as the input
  // |slot_span| pointer may be the result of an offset calculation and
  // therefore cannot be trusted. The objective of these functions is to
  // sanitize this input.
  ALWAYS_INLINE static void* ToSlotSpanStartPtr(
      const SlotSpanMetadata* slot_span);
  ALWAYS_INLINE static SlotSpanMetadata* FromSlotStartPtr(void* slot_start);
  ALWAYS_INLINE static SlotSpanMetadata* FromSlotInnerPtr(void* ptr);

  ALWAYS_INLINE PartitionSuperPageExtentEntry<thread_safe>* ToSuperPageExtent()
      const;

  // Checks if it is feasible to store raw_size.
  ALWAYS_INLINE bool CanStoreRawSize() const { return can_store_raw_size; }
  // The caller is responsible for ensuring that raw_size can be stored before
  // calling Set/GetRawSize.
  ALWAYS_INLINE void SetRawSize(size_t raw_size);
  ALWAYS_INLINE size_t GetRawSize() const;

  // The following functions get/set number of system pages that have been
  // previously committed by PartitionAlloc, even though not necessarily
  // committed now. Since those pages can only be decommitted using
  // PageKeepPermissionsIfPossible, the same option can be used to recommit them
  // (faster than PageUpdatePermissions).
  //
  // Do not call for direct map!
  ALWAYS_INLINE size_t GetPreviouslyCommittedSize() const {
    PA_DCHECK(!bucket->is_direct_mapped());
    // When |can_store_raw_size| is true (ie. single-slot spans, as direct map
    // doesn't go through this path), |num_previously_committed_system_pages|
    // stores 0 or 1, representing whether the entire slot of size |slot_size|
    // was committed.
    if (can_store_raw_size) {
      PA_DCHECK((bucket->slot_size & SystemPageOffsetMask()) == 0);
      return num_previously_committed_system_pages * bucket->slot_size;
    } else {
      return num_previously_committed_system_pages << SystemPageShift();
    }
  }
  ALWAYS_INLINE void IncreasePreviouslyCommittedSize(size_t size) {
    PA_DCHECK(!bucket->is_direct_mapped());
    PA_DCHECK(size > 0);
    PA_DCHECK((size & SystemPageOffsetMask()) == 0);
    // When |can_store_raw_size| is true (ie. single-slot spans, as direct map
    // doesn't go through this path), |num_previously_committed_system_pages|
    // stores 0 or 1, representing whether the entire slot of size |slot_size|
    // was committed.
    size_t num_pages = num_previously_committed_system_pages;
    if (can_store_raw_size) {
      PA_DCHECK(size == bucket->slot_size);
      // Single-slot spans can only be committed from 0 to its desired size,
      // so assert that. (This isn't true for direct map, due realloc, but
      // these don't go through this path.)
      PA_DCHECK(num_previously_committed_system_pages == 0);
      num_pages = 1;
    } else {
      num_pages += (size >> SystemPageShift());
    }
    PA_DCHECK(num_pages <= kMaxSystemPagesPerRegularSlotSpan);
    num_previously_committed_system_pages = static_cast<uint8_t>(num_pages);
    // Ensure no trimming happened.
    PA_DCHECK(num_pages == num_previously_committed_system_pages);
  }

  ALWAYS_INLINE void SetFreelistHead(PartitionFreelistEntry* new_head);

  // Returns size of the region used within a slot. The used region comprises
  // of actual allocated data, extras and possibly empty space in the middle.
  ALWAYS_INLINE size_t GetUtilizedSlotSize() const {
    // The returned size can be:
    // - The slot size for small buckets.
    // - Exact size needed to satisfy allocation (incl. extras), for large
    //   buckets and direct-mapped allocations (see also the comment in
    //   CanStoreRawSize() for more info).
    if (LIKELY(!CanStoreRawSize())) {
      return bucket->slot_size;
    }
    return GetRawSize();
  }

  // This includes padding due to rounding done at allocation; we don't know the
  // requested size at deallocation, so we use this in both places.
  ALWAYS_INLINE size_t GetSizeForBookkeeping() const {
    // This could be more precise for allocations where CanStoreRawSize()
    // returns true (large allocations). However this is called for *every*
    // allocation, so we don't want an extra branch there.
    return bucket->slot_size;
  }

  // Returns the size available to the app. It can be equal or higher than the
  // requested size. If higher, the overage won't exceed what's actually usable
  // by the app without a risk of running out of an allocated region or into
  // PartitionAlloc's internal data (like extras).
  ALWAYS_INLINE size_t GetUsableSize(PartitionRoot<thread_safe>* root) const {
    // The returned size can be:
    // - The slot size minus extras, for small buckets. This could be more than
    //   requested size.
    // - Raw size minus extras, for large buckets and direct-mapped allocations
    //   (see also the comment in CanStoreRawSize() for more info). This is
    //   equal to requested size.
    size_t size_to_ajdust;
    if (LIKELY(!CanStoreRawSize())) {
      size_to_ajdust = bucket->slot_size;
    } else {
      size_to_ajdust = GetRawSize();
    }
    return root->AdjustSizeForExtrasSubtract(size_to_ajdust);
  }

  // Returns the total size of the slots that are currently provisioned.
  ALWAYS_INLINE size_t GetProvisionedSize() const {
    size_t num_provisioned_slots =
        bucket->get_slots_per_span() - num_unprovisioned_slots;
    size_t provisioned_size = num_provisioned_slots * bucket->slot_size;
    PA_DCHECK(provisioned_size <= bucket->get_bytes_per_span());
    return provisioned_size;
  }

  ALWAYS_INLINE void Reset();

  // TODO(ajwong): Can this be made private?  https://crbug.com/787153
  BASE_EXPORT static SlotSpanMetadata* get_sentinel_slot_span();

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
  // sentinel_slot_span_ is used as a sentinel to indicate that there is no slot
  // span in the active list. We could use nullptr, but in that case we need to
  // add a null-check branch to the hot allocation path. We want to avoid that.
  //
  // Note, this declaration is kept in the header as opposed to an anonymous
  // namespace so the getter can be fully inlined.
  static SlotSpanMetadata sentinel_slot_span_;
  // For the sentinel.
  constexpr SlotSpanMetadata() noexcept
      : bucket(nullptr),
        can_store_raw_size(0),
        num_previously_committed_system_pages(0),
        unused(0) {}
};
static_assert(sizeof(SlotSpanMetadata<ThreadSafe>) <= kPageMetadataSize,
              "SlotSpanMetadata must fit into a Page Metadata slot.");

// Metadata of a non-first partition page in a slot span.
struct SubsequentPageMetadata {
  // Raw size is the size needed to satisfy the allocation (requested size +
  // extras). If available, it can be used to report better statistics or to
  // bring protective cookie closer to the allocated memory.
  //
  // It can be used only if:
  // - there is no more than one slot in the slot span (otherwise we wouldn't
  //   know which slot the raw size applies to)
  // - there is more than one partition page in the slot span (the metadata of
  //   the first one is used to store slot information, but the second one is
  //   available for extra information)
  size_t raw_size;
};

// Each partition page has metadata associated with it. The metadata of the
// first page of a slot span, describes that slot span. If a slot span spans
// more than 1 page, the page metadata may contain rudimentary additional
// information.
template <bool thread_safe>
struct PartitionPage {
  // "Pack" the union so that common page metadata still fits within
  // kPageMetadataSize. (SlotSpanMetadata is also "packed".)
  union __attribute__((packed)) {
    SlotSpanMetadata<thread_safe> slot_span_metadata;

    SubsequentPageMetadata subsequent_page_metadata;

    // sizeof(PartitionPageMetadata) must always be:
    // - a power of 2 (for fast modulo operations)
    // - below kPageMetadataSize
    //
    // This makes sure that this is respected no matter the architecture.
    char optional_padding[kPageMetadataSize - sizeof(uint8_t) - sizeof(bool)];
  };

  // The first PartitionPage of the slot span holds its metadata. This offset
  // tells how many pages in from that first page we are.
  // For direct maps, the first page metadata (that isn't super page extent
  // entry) uses this field to tell how many pages to the right the direct map
  // metadata starts.
  //
  // 6 bits is enough to represent all possible offsets, given that the smallest
  // partition page is 16kiB and the offset won't exceed 1MiB.
  static constexpr uint16_t kMaxSlotSpanMetadataBits = 6;
  static constexpr uint16_t kMaxSlotSpanMetadataOffset =
      (1 << kMaxSlotSpanMetadataBits) - 1;
  uint8_t slot_span_metadata_offset;

  // |is_valid| tells whether the page is part of a slot span. If |false|,
  // |has_valid_span_after_this| tells whether it's an unused region in between
  // slot spans within the super page.
  // Note, |is_valid| has been added for clarity, but if we ever need to save
  // this bit, it can be inferred from:
  //   |!slot_span_metadata_offset && slot_span_metadata->bucket|.
  bool is_valid : 1;
  bool has_valid_span_after_this : 1;

  ALWAYS_INLINE static PartitionPage* FromPtr(void* slot_start);

 private:
  ALWAYS_INLINE static void* ToSlotSpanStartPtr(const PartitionPage* page);
};

static_assert(sizeof(PartitionPage<ThreadSafe>) == kPageMetadataSize,
              "PartitionPage must be able to fit in a metadata slot");
static_assert(sizeof(PartitionPage<NotThreadSafe>) == kPageMetadataSize,
              "PartitionPage must be able to fit in a metadata slot");

// Certain functions rely on PartitionPage being either SlotSpanMetadata or
// SubsequentPageMetadata, and therefore freely casting between each other.
static_assert(offsetof(PartitionPage<ThreadSafe>, slot_span_metadata) == 0, "");
static_assert(offsetof(PartitionPage<ThreadSafe>, subsequent_page_metadata) ==
                  0,
              "");
static_assert(offsetof(PartitionPage<NotThreadSafe>, slot_span_metadata) == 0,
              "");
static_assert(offsetof(PartitionPage<NotThreadSafe>,
                       subsequent_page_metadata) == 0,
              "");

ALWAYS_INLINE char* PartitionSuperPageToMetadataArea(char* ptr) {
  PA_DCHECK(IsReservationStart(ptr));
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(ptr);
  PA_DCHECK(!(pointer_as_uint & kSuperPageOffsetMask));
  // The metadata area is exactly one system page (the guard page) into the
  // super page.
  return reinterpret_cast<char*>(pointer_as_uint + SystemPageSize());
}

template <bool thread_safe>
ALWAYS_INLINE PartitionSuperPageExtentEntry<thread_safe>*
PartitionSuperPageToExtent(char* ptr) {
  return reinterpret_cast<PartitionSuperPageExtentEntry<thread_safe>*>(
      PartitionSuperPageToMetadataArea(ptr));
}

// Size that should be reserved for 2 back-to-back quarantine bitmaps (if
// present) inside a super page. Elements of a super page are
// partition-page-aligned, hence the returned size is a multiple of partition
// page size.
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
ReservedQuarantineBitmapsSize() {
  return (2 * sizeof(QuarantineBitmap) + PartitionPageSize() - 1) &
         PartitionPageBaseMask();
}

// Size that should be committed for 2 back-to-back quarantine bitmaps (if
// present) inside a super page. It is a multiple of system page size.
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
CommittedQuarantineBitmapsSize() {
  return (2 * sizeof(QuarantineBitmap) + SystemPageSize() - 1) &
         SystemPageBaseMask();
}

// Returns the pointer to the first, of two, quarantine bitmap in the super
// page. It's the caller's responsibility to ensure that the bitmaps even exist.
ALWAYS_INLINE QuarantineBitmap* SuperPageQuarantineBitmaps(
    char* super_page_base) {
  PA_DCHECK(
      !(reinterpret_cast<uintptr_t>(super_page_base) % kSuperPageAlignment));
  return reinterpret_cast<QuarantineBitmap*>(super_page_base +
                                             PartitionPageSize());
}

ALWAYS_INLINE char* SuperPagePayloadBegin(char* super_page_base,
                                          bool with_quarantine) {
  PA_DCHECK(
      !(reinterpret_cast<uintptr_t>(super_page_base) % kSuperPageAlignment));
  return super_page_base + PartitionPageSize() +
         (with_quarantine ? ReservedQuarantineBitmapsSize() : 0);
}

ALWAYS_INLINE char* SuperPagePayloadEnd(char* super_page_base) {
  PA_DCHECK(
      !(reinterpret_cast<uintptr_t>(super_page_base) % kSuperPageAlignment));
  return super_page_base + kSuperPageSize - PartitionPageSize();
}

ALWAYS_INLINE size_t SuperPagePayloadSize(char* super_page_base,
                                          bool with_quarantine) {
  return SuperPagePayloadEnd(super_page_base) -
         SuperPagePayloadBegin(super_page_base, with_quarantine);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionSuperPageExtentEntry<
    thread_safe>::IncrementNumberOfNonemptySlotSpans() {
#if DCHECK_IS_ON()
  char* super_page_begin =
      base::bits::AlignDown(reinterpret_cast<char*>(this), kSuperPageAlignment);
  PA_DCHECK(
      (SuperPagePayloadSize(super_page_begin, root->IsQuarantineAllowed()) /
       PartitionPageSize()) > number_of_nonempty_slot_spans);
#endif
  ++number_of_nonempty_slot_spans;
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionSuperPageExtentEntry<
    thread_safe>::DecrementNumberOfNonemptySlotSpans() {
  PA_DCHECK(number_of_nonempty_slot_spans);
  --number_of_nonempty_slot_spans;
}

// Returns whether the pointer lies within a normal-bucket super page's payload
// area (i.e. area devoted to slot spans). It doesn't check whether it's within
// a valid slot span. It merely ensures it doesn't fall in a meta-data region
// that would surely never contain user data.
ALWAYS_INLINE bool IsWithinSuperPagePayload(void* ptr, bool with_quarantine) {
  PA_DCHECK(IsManagedByNormalBuckets(ptr));
  char* super_page_base = reinterpret_cast<char*>(
      reinterpret_cast<uintptr_t>(ptr) & kSuperPageBaseMask);
  void* payload_start = SuperPagePayloadBegin(super_page_base, with_quarantine);
  void* payload_end = SuperPagePayloadEnd(super_page_base);
  return ptr >= payload_start && ptr < payload_end;
}

// Returns the start of a slot, or nullptr if |maybe_inner_ptr| is not inside of
// an existing slot span. The function may return a non-nullptr pointer even
// inside a decommitted or free slot span, it's the caller responsibility to
// check if memory is actually allocated.
//
// |maybe_inner_ptr| must be within a normal-bucket super page, and more
// specifically within the payload area (i.e. area devoted to slot spans).
template <bool thread_safe>
ALWAYS_INLINE char* GetSlotStartInSuperPage(char* maybe_inner_ptr) {
#if DCHECK_IS_ON()
  PA_DCHECK(IsManagedByNormalBuckets(maybe_inner_ptr));
  char* super_page_ptr = reinterpret_cast<char*>(
      reinterpret_cast<uintptr_t>(maybe_inner_ptr) & kSuperPageBaseMask);
  auto* extent = reinterpret_cast<PartitionSuperPageExtentEntry<thread_safe>*>(
      PartitionSuperPageToMetadataArea(super_page_ptr));
  PA_DCHECK(IsWithinSuperPagePayload(maybe_inner_ptr,
                                     extent->root->IsQuarantineAllowed()));
#endif
  // Don't use FromSlotInnerPtr() because |is_valid| DCHECKs can fail there.
  auto* page = PartitionPage<thread_safe>::FromPtr(maybe_inner_ptr);
  if (!page->is_valid)
    return nullptr;
  page -= page->slot_span_metadata_offset;
  PA_DCHECK(page->is_valid);
  PA_DCHECK(!page->slot_span_metadata_offset);
  auto* slot_span = &page->slot_span_metadata;
  // Check if the slot span is actually used and valid.
  if (!slot_span->bucket)
    return nullptr;
#if DCHECK_IS_ON()
  PA_DCHECK(PartitionRoot<thread_safe>::IsValidSlotSpan(slot_span));
#endif
  char* const slot_span_begin = static_cast<char*>(
      SlotSpanMetadata<thread_safe>::ToSlotSpanStartPtr(slot_span));
  const ptrdiff_t ptr_offset = maybe_inner_ptr - slot_span_begin;
#if DCHECK_IS_ON()
  PA_DCHECK(0 <= ptr_offset &&
            ptr_offset < static_cast<ptrdiff_t>(
                             slot_span->bucket->get_pages_per_slot_span() *
                             PartitionPageSize()));
#endif
  // Slot span size in bytes is not necessarily multiple of partition page.
  if (ptr_offset >=
      static_cast<ptrdiff_t>(slot_span->bucket->get_bytes_per_span()))
    return nullptr;
  const size_t slot_size = slot_span->bucket->slot_size;
  const size_t slot_number = slot_span->bucket->GetSlotNumber(ptr_offset);
  char* const result = slot_span_begin + (slot_number * slot_size);
  PA_DCHECK(result <= maybe_inner_ptr && maybe_inner_ptr < result + slot_size);
  return result;
}

// Converts from a pointer to the PartitionPage object (within super pages's
// metadata) into a pointer to the beginning of the slot span.
// |page| must be the first PartitionPage of the slot span.
template <bool thread_safe>
ALWAYS_INLINE void* PartitionPage<thread_safe>::ToSlotSpanStartPtr(
    const PartitionPage* page) {
  PA_DCHECK(page->is_valid);
  PA_DCHECK(!page->slot_span_metadata_offset);
  return SlotSpanMetadata<thread_safe>::ToSlotSpanStartPtr(
      &page->slot_span_metadata);
}

// Converts from a pointer inside a super page into a pointer to the
// PartitionPage object (within super pages's metadata) that describes the
// partition page where |ptr| is located. |ptr| doesn't have to be located
// within a valid (i.e. allocated) slot span, but must be within the super
// page's payload area (i.e. area devoted to slot spans).
//
// While it is generally valid for |ptr| to be in the middle of an allocation,
// care has to be taken with direct maps that span multiple super pages. This
// function's behavior is undefined if |ptr| lies in a subsequent super page.
template <bool thread_safe>
ALWAYS_INLINE PartitionPage<thread_safe>* PartitionPage<thread_safe>::FromPtr(
    void* ptr) {
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(ptr);
  char* super_page_ptr =
      reinterpret_cast<char*>(pointer_as_uint & kSuperPageBaseMask);

#if DCHECK_IS_ON()
  PA_DCHECK(IsReservationStart(super_page_ptr));
  if (IsManagedByNormalBuckets(ptr)) {
    auto* extent =
        reinterpret_cast<PartitionSuperPageExtentEntry<thread_safe>*>(
            PartitionSuperPageToMetadataArea(super_page_ptr));
    PA_DCHECK(
        IsWithinSuperPagePayload(ptr, extent->root->IsQuarantineAllowed()));
  } else {
    PA_CHECK(ptr >= super_page_ptr + PartitionPageSize());
  }
#endif

  uintptr_t partition_page_index =
      (pointer_as_uint & kSuperPageOffsetMask) >> PartitionPageShift();
  // Index 0 is invalid because it is the super page extent metadata and the
  // last index is invalid because the whole PartitionPage is set as guard
  // pages. This repeats part of the payload PA_DCHECK above, which may check
  // for other exclusions.
  PA_DCHECK(partition_page_index);
  PA_DCHECK(partition_page_index < NumPartitionPagesPerSuperPage() - 1);
  return reinterpret_cast<PartitionPage<thread_safe>*>(
      PartitionSuperPageToMetadataArea(super_page_ptr) +
      (partition_page_index << kPageMetadataShift));
}

// Converts from a pointer to the SlotSpanMetadata object (within a super
// pages's metadata) into a pointer to the beginning of the slot span. This
// works on direct maps too.
template <bool thread_safe>
ALWAYS_INLINE void* SlotSpanMetadata<thread_safe>::ToSlotSpanStartPtr(
    const SlotSpanMetadata* slot_span) {
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(slot_span);
  uintptr_t super_page_offset = (pointer_as_uint & kSuperPageOffsetMask);

  // A valid |page| must be past the first guard System page and within
  // the following metadata region.
  PA_DCHECK(super_page_offset > SystemPageSize());
  // Must be less than total metadata region.
  PA_DCHECK(super_page_offset <
            SystemPageSize() +
                (NumPartitionPagesPerSuperPage() * kPageMetadataSize));
  uintptr_t partition_page_index =
      (super_page_offset - SystemPageSize()) >> kPageMetadataShift;
  // Index 0 is invalid because it is the super page extent metadata and the
  // last index is invalid because the whole PartitionPage is set as guard
  // pages.
  PA_DCHECK(partition_page_index);
  PA_DCHECK(partition_page_index < NumPartitionPagesPerSuperPage() - 1);
  uintptr_t super_page_base = (pointer_as_uint & kSuperPageBaseMask);
  void* ret = reinterpret_cast<void*>(
      super_page_base + (partition_page_index << PartitionPageShift()));
  return ret;
}

// Converts from a pointer inside a slot into a pointer to the SlotSpanMetadata
// object (within super pages's metadata) that describes the slot span
// containing that slot.
//
// CAUTION! For direct-mapped allocation, |ptr| has to be within the first
// partition page.
template <bool thread_safe>
ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::FromSlotInnerPtr(void* ptr) {
  auto* page = PartitionPage<thread_safe>::FromPtr(ptr);
  PA_DCHECK(page->is_valid);
  // Partition pages in the same slot span share the same slot span metadata
  // object (located in the first PartitionPage object of that span). Adjust
  // for that.
  page -= page->slot_span_metadata_offset;
  PA_DCHECK(page->is_valid);
  PA_DCHECK(!page->slot_span_metadata_offset);
  auto* slot_span = &page->slot_span_metadata;
  // For direct map, if |ptr| doesn't point within the first partition page,
  // |slot_span_metadata_offset| will be 0, |page| won't get shifted, leaving
  // |slot_size| at 0.
  PA_DCHECK(slot_span->bucket->slot_size);
  return slot_span;
}

template <bool thread_safe>
ALWAYS_INLINE PartitionSuperPageExtentEntry<thread_safe>*
SlotSpanMetadata<thread_safe>::ToSuperPageExtent() const {
  char* super_page_base = reinterpret_cast<char*>(
      reinterpret_cast<uintptr_t>(this) & kSuperPageBaseMask);
  return reinterpret_cast<PartitionSuperPageExtentEntry<thread_safe>*>(
      PartitionSuperPageToMetadataArea(super_page_base));
}

// Like |FromSlotInnerPtr|, but asserts that pointer points to the beginning of
// the slot. This works on direct maps too.
template <bool thread_safe>
ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::FromSlotStartPtr(void* slot_start) {
  auto* slot_span = FromSlotInnerPtr(slot_start);
  // Checks that the pointer is a multiple of slot size.
  auto* slot_span_start = ToSlotSpanStartPtr(slot_span);
  PA_DCHECK(!((reinterpret_cast<uintptr_t>(slot_start) -
               reinterpret_cast<uintptr_t>(slot_span_start)) %
              slot_span->bucket->slot_size));
  return slot_span;
}

template <bool thread_safe>
ALWAYS_INLINE void SlotSpanMetadata<thread_safe>::SetRawSize(size_t raw_size) {
  PA_DCHECK(CanStoreRawSize());
  auto* the_next_page = reinterpret_cast<PartitionPage<thread_safe>*>(this) + 1;
  the_next_page->subsequent_page_metadata.raw_size = raw_size;
}

template <bool thread_safe>
ALWAYS_INLINE size_t SlotSpanMetadata<thread_safe>::GetRawSize() const {
  PA_DCHECK(CanStoreRawSize());
  auto* the_next_page =
      reinterpret_cast<const PartitionPage<thread_safe>*>(this) + 1;
  return the_next_page->subsequent_page_metadata.raw_size;
}

template <bool thread_safe>
ALWAYS_INLINE void SlotSpanMetadata<thread_safe>::SetFreelistHead(
    PartitionFreelistEntry* new_head) {
  PA_DCHECK(!new_head ||
            (reinterpret_cast<uintptr_t>(this) & kSuperPageBaseMask) ==
                (reinterpret_cast<uintptr_t>(new_head) & kSuperPageBaseMask));
  freelist_head = new_head;
}

template <bool thread_safe>
ALWAYS_INLINE DeferredUnmap
SlotSpanMetadata<thread_safe>::Free(void* slot_start) EXCLUSIVE_LOCKS_REQUIRED(
    PartitionRoot<thread_safe>::FromSlotSpan(this)->lock_) {
#if DCHECK_IS_ON()
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(this);
  root->lock_.AssertAcquired();
#endif

  PA_DCHECK(num_allocated_slots);
  // Catches an immediate double free.
  PA_CHECK(slot_start != freelist_head);
  // Look for double free one level deeper in debug.
  PA_DCHECK(!freelist_head ||
            slot_start != freelist_head->GetNext(bucket->slot_size));
  auto* entry = static_cast<internal::PartitionFreelistEntry*>(slot_start);
  entry->SetNext(freelist_head);
  SetFreelistHead(entry);
  --num_allocated_slots;
  if (UNLIKELY(num_allocated_slots <= 0)) {
    return FreeSlowPath();
  } else {
    // All single-slot allocations must go through the slow path to
    // correctly update the raw size.
    PA_DCHECK(!CanStoreRawSize());
  }
  return {};
}

template <bool thread_safe>
ALWAYS_INLINE bool SlotSpanMetadata<thread_safe>::is_active() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  return (num_allocated_slots > 0 &&
          (freelist_head || num_unprovisioned_slots));
}

template <bool thread_safe>
ALWAYS_INLINE bool SlotSpanMetadata<thread_safe>::is_full() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  bool ret = (num_allocated_slots == bucket->get_slots_per_span());
  if (ret) {
    PA_DCHECK(!freelist_head);
    PA_DCHECK(!num_unprovisioned_slots);
  }
  return ret;
}

template <bool thread_safe>
ALWAYS_INLINE bool SlotSpanMetadata<thread_safe>::is_empty() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  return (!num_allocated_slots && freelist_head);
}

template <bool thread_safe>
ALWAYS_INLINE bool SlotSpanMetadata<thread_safe>::is_decommitted() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  bool ret = (!num_allocated_slots && !freelist_head);
  if (ret) {
    PA_DCHECK(!num_unprovisioned_slots);
    PA_DCHECK(empty_cache_index == -1);
  }
  return ret;
}

template <bool thread_safe>
ALWAYS_INLINE void SlotSpanMetadata<thread_safe>::Reset() {
  PA_DCHECK(is_decommitted());

  num_unprovisioned_slots = bucket->get_slots_per_span();
  PA_DCHECK(num_unprovisioned_slots);

  ToSuperPageExtent()->IncrementNumberOfNonemptySlotSpans();

  next_slot_span = nullptr;
}

ALWAYS_INLINE void DeferredUnmap::Run() {
  if (UNLIKELY(reservation_start)) {
    Unmap();
  }
}

enum class QuarantineBitmapType { kMutator, kScanner };

// Returns a quarantine bitmap from a pointer within a normal-bucket super page.
ALWAYS_INLINE QuarantineBitmap* QuarantineBitmapFromPointer(
    QuarantineBitmapType type,
    size_t pcscan_epoch,
    void* ptr) {
  PA_DCHECK(IsManagedByNormalBuckets(ptr));
  auto* super_page_base = reinterpret_cast<char*>(
      reinterpret_cast<uintptr_t>(ptr) & kSuperPageBaseMask);
  auto* first_bitmap = SuperPageQuarantineBitmaps(super_page_base);
  auto* second_bitmap = first_bitmap + 1;

  if (type == QuarantineBitmapType::kScanner)
    std::swap(first_bitmap, second_bitmap);

  return (pcscan_epoch & 1) ? second_bitmap : first_bitmap;
}

// Iterates over all slot spans in a super-page. |Callback| must return true if
// early return is needed.
template <bool thread_safe, typename Callback>
void IterateSlotSpans(char* super_page_base,
                      bool with_quarantine,
                      Callback callback) {
#if DCHECK_IS_ON()
  PA_DCHECK(
      !(reinterpret_cast<uintptr_t>(super_page_base) % kSuperPageAlignment));
  auto* extent_entry =
      reinterpret_cast<PartitionSuperPageExtentEntry<thread_safe>*>(
          PartitionSuperPageToMetadataArea(super_page_base));
  extent_entry->root->lock_.AssertAcquired();
#endif

  using Page = PartitionPage<thread_safe>;
  using SlotSpan = SlotSpanMetadata<thread_safe>;
  auto* const first_page =
      Page::FromPtr(SuperPagePayloadBegin(super_page_base, with_quarantine));
  auto* const last_page =
      Page::FromPtr(SuperPagePayloadEnd(super_page_base) - PartitionPageSize());
  Page* page;
  SlotSpan* slot_span;
  for (page = first_page; page <= last_page;) {
    PA_DCHECK(!page->slot_span_metadata_offset);  // Ensure slot span beginning.
    if (!page->is_valid) {
      if (page->has_valid_span_after_this) {
        // The page doesn't represent a valid slot span, but there is another
        // one somewhere after this. Keep iterating to find it.
        ++page;
        continue;
      }
      // There are currently no valid spans from here on. No need to iterate
      // the rest of the super page.
      break;
    }
    slot_span = &page->slot_span_metadata;
    if (callback(slot_span))
      return;
    page += slot_span->bucket->get_pages_per_slot_span();
  }
  // Each super page must have at least one valid slot span.
  PA_DCHECK(page > first_page);
  // Just a quick check that the search ended at a valid slot span and there
  // was no unnecessary iteration over gaps afterwards.
  PA_DCHECK(page == reinterpret_cast<Page*>(slot_span) +
                        slot_span->bucket->get_pages_per_slot_span());
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_
