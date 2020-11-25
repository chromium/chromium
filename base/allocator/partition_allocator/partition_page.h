// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_

#include <string.h>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/object_bitmap.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
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

// SlotSpanMetadata::Free() defers unmapping a large page until the lock is
// released. Callers of SlotSpanMetadata::Free() must invoke Run().
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

using QuarantineBitmap =
    ObjectBitmap<kSuperPageSize, kSuperPageAlignment, kAlignment>;

// Metadata of the slot span.
//
// Some notes on slot span states. It can be in one of four major states:
// 1) Active.
// 2) Full.
// 3) Empty.
// 4) Decommitted.
// An active slot span has available free slots. A full slot span has no free
// slots. An empty slot span has no free slots, and a decommitted slot span is
// an empty one that had its backing memory released back to the system.
//
// There are two linked lists tracking slot spans. The "active" list is an
// approximation of a list of active slot spans. It is an approximation because
// full, empty and decommitted slot spans may briefly be present in the list
// until we next do a scan over it. The "empty" list is an accurate list of slot
// spans which are either empty or decommitted.
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
//   from the empty list on to the active list.
template <bool thread_safe>
struct __attribute__((packed)) SlotSpanMetadata {
  PartitionFreelistEntry* freelist_head;
  SlotSpanMetadata<thread_safe>* next_slot_span;
  PartitionBucket<thread_safe>* bucket;

  // Deliberately signed, 0 for empty or decommitted slot spans, -n for full
  // slot spans:
  int16_t num_allocated_slots;
  uint16_t num_unprovisioned_slots;
  int16_t empty_cache_index;  // -1 if not in the empty cache.

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
  ALWAYS_INLINE static void* ToPointer(const SlotSpanMetadata* slot_span);
  ALWAYS_INLINE static SlotSpanMetadata* FromPointer(void* ptr);
  ALWAYS_INLINE static SlotSpanMetadata* FromPointerNoAlignmentCheck(void* ptr);

  // Checks if it is feasible to store raw_size.
  ALWAYS_INLINE bool CanStoreRawSize() const;
  // The caller is responsible for ensuring that raw_size can be stored before
  // calling Set/GetRawSize.
  ALWAYS_INLINE void SetRawSize(size_t raw_size);
  ALWAYS_INLINE size_t GetRawSize() const;

  ALWAYS_INLINE void SetFreelistHead(PartitionFreelistEntry* new_head);

  // Returns size of the region used within a slot. The used region comprises
  // of actual allocated data, extras and possibly empty space.
  ALWAYS_INLINE size_t GetUtilizedSlotSize() const {
    // The returned size can be:
    // - The slot size for small buckets.
    // - Exact needed size to satisfy allocation (incl. extras), for large
    //   buckets and direct-mapped allocations (see also the comment in
    //   CanStoreRawSize() for more info).
    if (LIKELY(!CanStoreRawSize()))
      return bucket->slot_size;
    return GetRawSize();
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
};

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
  // "Pack" the union so that slot_span_metadata_offset still fits within
  // kPageMetadataSize. (SlotSpanMetadata is also "packed".)
  union __attribute__((packed)) {
    SlotSpanMetadata<thread_safe> slot_span_metadata;

    SubsequentPageMetadata subsequent_page_metadata;

    // sizeof(PartitionPageMetadata) must always be:
    // - a power of 2 (for fast modulo operations)
    // - below kPageMetadataSize
    //
    // This makes sure that this is respected no matter the architecture.
    char optional_padding[kPageMetadataSize - sizeof(uint16_t)];
  };

  // The first PartitionPage of the slot span holds its metadata. This offset
  // tells how many pages in from that first page we are.
  uint16_t slot_span_metadata_offset;

  ALWAYS_INLINE static PartitionPage* FromPointerNoAlignmentCheck(void* ptr);
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
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(ptr);
  PA_DCHECK(!(pointer_as_uint & kSuperPageOffsetMask));
  // The metadata area is exactly one system page (the guard page) into the
  // super page.
  return reinterpret_cast<char*>(pointer_as_uint + SystemPageSize());
}

// Size that should be reserved for 2 back-to-back quarantine bitmaps (if
// present) inside a super page. Elements of a super page are
// partition-page-aligned, hence the returned size is a multiple of partition
// page size.
ALWAYS_INLINE size_t ReservedQuarantineBitmapsSize() {
  return (2 * sizeof(QuarantineBitmap) + PartitionPageSize() - 1) &
         PartitionPageBaseMask();
}

// Size that should be committed for 2 back-to-back quarantine bitmaps (if
// present) inside a super page. It is a multiple of system page size.
ALWAYS_INLINE size_t CommittedQuarantineBitmapsSize() {
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
                                          bool with_pcscan) {
  PA_DCHECK(
      !(reinterpret_cast<uintptr_t>(super_page_base) % kSuperPageAlignment));
  return super_page_base + PartitionPageSize() +
         (with_pcscan ? ReservedQuarantineBitmapsSize() : 0);
}

ALWAYS_INLINE char* SuperPagePayloadEnd(char* super_page_base) {
  PA_DCHECK(
      !(reinterpret_cast<uintptr_t>(super_page_base) % kSuperPageAlignment));
  return super_page_base + kSuperPageSize - PartitionPageSize();
}

ALWAYS_INLINE bool IsWithinSuperPagePayload(char* ptr, bool with_pcscan) {
  PA_DCHECK(!IsManagedByPartitionAllocDirectMap(ptr));
  char* super_page_base = reinterpret_cast<char*>(
      reinterpret_cast<uintptr_t>(ptr) & kSuperPageBaseMask);
  char* payload_start = SuperPagePayloadBegin(super_page_base, with_pcscan);
  char* payload_end = SuperPagePayloadEnd(super_page_base);
  return ptr >= payload_start && ptr < payload_end;
}

// Returns the start of a slot or nullptr if |maybe_inner_ptr| is not inside of
// an existing slot span. The function may return a pointer even inside a
// decommitted or free slot span, it's the caller responsibility to check if
// memory is actually allocated.
// The precondition is that |maybe_inner_ptr| must point to payload of a valid
// super page.
template <bool thread_safe>
ALWAYS_INLINE char* GetSlotStartInSuperPage(char* maybe_inner_ptr) {
  PA_DCHECK(!IsManagedByPartitionAllocDirectMap(maybe_inner_ptr));
  char* super_page_ptr = reinterpret_cast<char*>(
      reinterpret_cast<uintptr_t>(maybe_inner_ptr) & kSuperPageBaseMask);
  auto* extent = reinterpret_cast<PartitionSuperPageExtentEntry<thread_safe>*>(
      PartitionSuperPageToMetadataArea(super_page_ptr));
  PA_DCHECK(
      IsWithinSuperPagePayload(maybe_inner_ptr, extent->root->IsScanEnabled()));
  auto* slot_span = SlotSpanMetadata<thread_safe>::FromPointerNoAlignmentCheck(
      maybe_inner_ptr);
  // Check if the slot span is actually used and valid.
  if (!slot_span->bucket)
    return nullptr;
  PA_DCHECK(PartitionRoot<thread_safe>::IsValidSlotSpan(slot_span));
  char* const slot_span_begin =
      static_cast<char*>(SlotSpanMetadata<thread_safe>::ToPointer(slot_span));
  const ptrdiff_t ptr_offset = maybe_inner_ptr - slot_span_begin;
  PA_DCHECK(0 <= ptr_offset &&
            ptr_offset < static_cast<ptrdiff_t>(
                             slot_span->bucket->get_pages_per_slot_span() *
                             PartitionPageSize()));
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

// See the comment for |FromPointer|.
template <bool thread_safe>
ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::FromPointerNoAlignmentCheck(void* ptr) {
  return &PartitionPage<thread_safe>::FromPointerNoAlignmentCheck(ptr)
              ->slot_span_metadata;
}

// See the comment for |FromPointer|.
template <bool thread_safe>
ALWAYS_INLINE PartitionPage<thread_safe>*
PartitionPage<thread_safe>::FromPointerNoAlignmentCheck(void* ptr) {
  uintptr_t pointer_as_uint = reinterpret_cast<uintptr_t>(ptr);
  char* super_page_ptr =
      reinterpret_cast<char*>(pointer_as_uint & kSuperPageBaseMask);
  uintptr_t partition_page_index =
      (pointer_as_uint & kSuperPageOffsetMask) >> PartitionPageShift();
  // Index 0 is invalid because it is the super page extent metadata and the
  // last index is invalid because the whole PartitionPage is set as guard
  // pages.
  PA_DCHECK(partition_page_index);
  PA_DCHECK(partition_page_index < NumPartitionPagesPerSuperPage() - 1);
  auto* page = reinterpret_cast<PartitionPage<thread_safe>*>(
      PartitionSuperPageToMetadataArea(super_page_ptr) +
      (partition_page_index << kPageMetadataShift));
  // Partition pages in the same slot span share the same slot span metadata
  // object (located in the first PartitionPage object of that span). Adjust
  // for that.
  page -= page->slot_span_metadata_offset;
  return page;
}

// Converts from a pointer to the SlotSpanMetadata object (within super pages's
// metadata) into a pointer to the beginning of the slot span.
template <bool thread_safe>
ALWAYS_INLINE void* SlotSpanMetadata<thread_safe>::ToPointer(
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

// Converts from a pointer inside a slot span into a pointer to the
// SlotSpanMetadata object (within super pages's metadata).
template <bool thread_safe>
ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
SlotSpanMetadata<thread_safe>::FromPointer(void* ptr) {
  SlotSpanMetadata* slot_span =
      SlotSpanMetadata::FromPointerNoAlignmentCheck(ptr);
  // Checks that the pointer is a multiple of slot size.
  PA_DCHECK(
      !((reinterpret_cast<uintptr_t>(ptr) -
         reinterpret_cast<uintptr_t>(SlotSpanMetadata::ToPointer(slot_span))) %
        slot_span->bucket->slot_size));
  return slot_span;
}

template <bool thread_safe>
ALWAYS_INLINE bool SlotSpanMetadata<thread_safe>::CanStoreRawSize() const {
  // For direct-map as well as single-slot slot spans (recognized by checking
  // against |kMaxPartitionPagesPerSlotSpan|), we have some spare metadata space
  // in subsequent PartitionPage to store the raw size. It isn't only metadata
  // space though, slot spans that have more than one slot can't have raw size
  // stored, because we wouldn't know which slot it applies to.
  if (LIKELY(bucket->slot_size <=
             MaxSystemPagesPerSlotSpan() * SystemPageSize()))
    return false;

  PA_DCHECK((bucket->slot_size % SystemPageSize()) == 0);
  PA_DCHECK(bucket->is_direct_mapped() || bucket->get_slots_per_span() == 1);

  return true;
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
ALWAYS_INLINE DeferredUnmap SlotSpanMetadata<thread_safe>::Free(void* ptr) {
#if DCHECK_IS_ON()
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(this);
  root->lock_.AssertAcquired();
#endif

  PA_DCHECK(num_allocated_slots);
  // Catches an immediate double free.
  PA_CHECK(ptr != freelist_head);
  // Look for double free one level deeper in debug.
  PA_DCHECK(!freelist_head || ptr != freelist_head->GetNext());
  auto* entry = static_cast<internal::PartitionFreelistEntry*>(ptr);
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

  next_slot_span = nullptr;
}

ALWAYS_INLINE void DeferredUnmap::Run() {
  if (UNLIKELY(ptr)) {
    Unmap();
  }
}

enum class QuarantineBitmapType { kMutator, kScanner };

ALWAYS_INLINE QuarantineBitmap* QuarantineBitmapFromPointer(
    QuarantineBitmapType type,
    size_t pcscan_epoch,
    void* ptr) {
  PA_DCHECK(!IsManagedByPartitionAllocDirectMap(ptr));
  auto* super_page_base = reinterpret_cast<char*>(
      reinterpret_cast<uintptr_t>(ptr) & kSuperPageBaseMask);
  auto* first_bitmap = SuperPageQuarantineBitmaps(super_page_base);
  auto* second_bitmap = first_bitmap + 1;

  if (type == QuarantineBitmapType::kScanner)
    std::swap(first_bitmap, second_bitmap);

  return (pcscan_epoch & 1) ? second_bitmap : first_bitmap;
}

template <bool thread_safe, typename Callback>
void IterateActiveAndFullSlotSpans(char* super_page_base,
                                   bool with_pcscan,
                                   Callback callback) {
  PA_DCHECK(
      !(reinterpret_cast<uintptr_t>(super_page_base) % kSuperPageAlignment));
#if DCHECK_IS_ON()
  auto* extent_entry =
      reinterpret_cast<PartitionSuperPageExtentEntry<thread_safe>*>(
          PartitionSuperPageToMetadataArea(super_page_base));
  extent_entry->root->lock_.AssertAcquired();
#endif

  using Page = PartitionPage<thread_safe>;
  auto* const first_page = Page::FromPointerNoAlignmentCheck(
      SuperPagePayloadBegin(super_page_base, with_pcscan));
  auto* const last_page = Page::FromPointerNoAlignmentCheck(
      SuperPagePayloadEnd(super_page_base) - PartitionPageSize());
  for (auto* page = first_page;
       page <= last_page && page->slot_span_metadata.bucket;
       page += page->slot_span_metadata.bucket->get_pages_per_slot_span()) {
    auto* slot_span = &page->slot_span_metadata;
    if (slot_span->is_empty() || slot_span->is_decommitted()) {
      continue;
    }
    callback(slot_span);
  }
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_PAGE_H_
