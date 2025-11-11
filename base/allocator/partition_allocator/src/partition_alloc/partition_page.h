// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef PARTITION_ALLOC_PARTITION_PAGE_H_
#define PARTITION_ALLOC_PARTITION_PAGE_H_

#include <cstdint>

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/address_pool_manager_types.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc-inl.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_bucket.h"
#include "partition_alloc/partition_dcheck_helper.h"
#include "partition_alloc/partition_freelist_entry.h"
#include "partition_alloc/partition_page_constants.h"
#include "partition_alloc/partition_superpage_extent_entry.h"
#include "partition_alloc/reservation_offset_table.h"
#include "partition_alloc/slot_start.h"

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
#include "partition_alloc/tagging.h"
#endif

namespace partition_alloc::internal {

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
#pragma pack(push, 1)
struct SlotSpanMetadata {
 private:
  FreelistEntry* freelist_head = nullptr;

 public:
  // TODO(lizeb): Make as many fields as possible private or const, to
  // encapsulate things more clearly.
  SlotSpanMetadata* next_slot_span = nullptr;
  PartitionBucket* const bucket = nullptr;

  // CHECK()ed in AllocNewSlotSpan().
  // The maximum number of bits needed to cover all currently supported OSes.
  static constexpr size_t kMaxSlotsPerSlotSpanBits = 15;
  static_assert(kMaxSlotsPerSlotSpan < (1 << kMaxSlotsPerSlotSpanBits), "");

  // |num_allocated_slots| is 0 for empty or decommitted slot spans, which can
  // be further differentiated by checking existence of the freelist.
  uint32_t num_allocated_slots : kMaxSlotsPerSlotSpanBits;
  uint32_t num_unprovisioned_slots : kMaxSlotsPerSlotSpanBits;

  // |marked_full| isn't equivalent to being full. Slot span is marked as full
  // iff it isn't on the active slot span list (or any other list).
  uint32_t marked_full : 1;

 private:
  const uint32_t can_store_raw_size_ : 1;
  uint16_t freelist_is_sorted_ : 1;
  // If |in_empty_cache_|==1, |empty_cache_index| is undefined and mustn't be
  // used.
  uint16_t in_empty_cache_ : 1;
  // Index of the page in the empty cache. This is in the range
  // [0, `kMaxEmptySlotSpanRingSize - 1`] so it fits in
  // `BitWidth(kMaxEmptySlotSpanRingSize - 1)`.
  uint16_t empty_cache_index_
      : internal::base::bits::BitWidth(kMaxEmptySlotSpanRingSize - 1);
  // Can use only 48 bits (6B) in this bitfield, as this structure is embedded
  // in PartitionPage which has 2B worth of fields and must fit in 32B.

 public:
  // Checks if it is feasible to store raw_size.
  PA_ALWAYS_INLINE bool CanStoreRawSize() const {
#if defined(THREAD_SANITIZER)
    // `can_store_raw_size_` is a cache of the value in the bucket, stored there
    // to avoid touching `bucket`. It causes issues with TSAN, since it is part
    // of a bitfield along with non-constant values.
    //
    // The warning is correct, though it is extremely unlikely to cause issues
    // in practice, as the element in the bitfield is constant, so the only case
    // where it would cause issue is when a non-atomic read to a variable can
    // give garbage results, where the bits are neither from the old nor the new
    // value.
    //
    // TODO(crbug.com/437026570): Fix that properly, rather than relying on
    // effectively a suppression.
    return bucket->CanStoreRawSize();
#else
    return can_store_raw_size_;
#endif
  }

  // Returns the total size of the slots that are currently provisioned.
  PA_ALWAYS_INLINE size_t GetProvisionedSize() const {
    size_t num_provisioned_slots =
        bucket->get_slots_per_span() - num_unprovisioned_slots;
    size_t provisioned_size = num_provisioned_slots * bucket->slot_size;
    PA_DCHECK(provisioned_size <= bucket->get_bytes_per_span());
    return provisioned_size;
  }

  // Return the number of entries in the freelist.
  size_t GetFreelistLength() const {
    size_t num_provisioned_slots =
        bucket->get_slots_per_span() - num_unprovisioned_slots;
    return num_provisioned_slots - num_allocated_slots;
  }

  PA_ALWAYS_INLINE bool in_empty_cache() const { return in_empty_cache_; }

  PA_COMPONENT_EXPORT(PARTITION_ALLOC)
  explicit SlotSpanMetadata(PartitionBucket* bucket);

  // pa_tcache_inspect needs the copy constructor.
  inline SlotSpanMetadata(const SlotSpanMetadata&);

  // Public API
  // Pointer/address manipulation functions. These must be static as the input
  // |slot_span| pointer may be the result of an offset calculation and
  // therefore cannot be trusted. The objective of these functions is to
  // sanitize this input.

  // If metadata is outside of GigaCage, the following methods:
  //  ToSlotSpanStart(), FromAddr(), FromSlotStart(), FromObject(),
  //  FromObjectInnerAddr(), and FromObjectInnerAddr()
  // are much slower, because:
  // (1) need to find `pool` from the given address,
  // (2) need to obtain `offset` from the `pool`.
  // If `PartitionRoot` is known or metadata offset is known, it is much better
  // to use methods with `root` or `offset`.
  // For example, `FromAddr(uintptr_t address)` is slow and not recommended.
  // `FromAddr(uintptr_t address, const PartitionRoot*)` takes a `root`
  // parameter and `FromAddr(uintptr_t address, std::ptrdiff offset)` takes an
  // `offset` parameter, which are recommended 146-147.
  PA_ALWAYS_INLINE static SlotSpanStart ToSlotSpanStart(
      const SlotSpanMetadata* slot_span,
      [[maybe_unused]] const PartitionRoot* root);
  PA_ALWAYS_INLINE static SlotSpanStart ToSlotSpanStart(
      const SlotSpanMetadata* slot_span,
      [[maybe_unused]] std::ptrdiff_t offset);

  PA_ALWAYS_INLINE static SlotSpanMetadata* FromAddr(uintptr_t address);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromAddr(
      uintptr_t address,
      [[maybe_unused]] const PartitionRoot* root);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromAddr(
      uintptr_t address,
      [[maybe_unused]] std::ptrdiff_t offset);

  PA_ALWAYS_INLINE static SlotSpanMetadata* FromSlotStart(
      UntaggedSlotStart slot_start);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromSlotStart(
      UntaggedSlotStart slot_start,
      [[maybe_unused]] const PartitionRoot* root);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromSlotStart(
      UntaggedSlotStart slot_start,
      [[maybe_unused]] std::ptrdiff_t offset);

  PA_ALWAYS_INLINE static SlotSpanMetadata* FromObjectInnerAddr(
      uintptr_t address,
      [[maybe_unused]] const PartitionRoot* root);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromObjectInnerAddr(
      uintptr_t address,
      [[maybe_unused]] std::ptrdiff_t offset);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromObjectInnerPtr(
      const void* ptr,
      [[maybe_unused]] const PartitionRoot* root);
  PA_ALWAYS_INLINE static SlotSpanMetadata* FromObjectInnerPtr(
      const void* ptr,
      [[maybe_unused]] std::ptrdiff_t offset);

  PA_ALWAYS_INLINE PartitionSuperPageExtentEntry* ToSuperPageExtent() const;

  PA_ALWAYS_INLINE size_t GetRawSize() const;

  PA_ALWAYS_INLINE FreelistEntry* get_freelist_head() const {
    return freelist_head;
  }

  // Returns size of the region used within a slot. The used region comprises
  // of actual allocated data, extras and possibly empty space in the middle.
  PA_ALWAYS_INLINE size_t GetUtilizedSlotSize() const {
    // The returned size can be:
    // - The slot size for small buckets.
    // - Exact size needed to satisfy allocation (incl. extras), for large
    //   buckets and direct-mapped allocations (see also the comment in
    //   CanStoreRawSize() for more info).
    if (!CanStoreRawSize()) [[likely]] {
      return bucket->slot_size;
    }
    return GetRawSize();
  }

  // This includes padding due to rounding done at allocation; we don't know the
  // requested size at deallocation, so we use this in both places.
  PA_ALWAYS_INLINE size_t GetSlotSizeForBookkeeping() const {
    // This could be more precise for allocations where CanStoreRawSize()
    // returns true (large allocations). However this is called for *every*
    // allocation, so we don't want an extra branch there.
    return bucket->slot_size;
  }

  // TODO(ajwong): Can this be made private?  https://crbug.com/787153
  PA_COMPONENT_EXPORT(PARTITION_ALLOC)
  static const SlotSpanMetadata* get_sentinel_slot_span();
  // The sentinel is not supposed to be modified and hence we mark it as const
  // under the hood. However, we often store it together with mutable metadata
  // objects and need a non-const pointer.
  // You can use this function for this case, but you need to ensure that the
  // returned object will not be written to.
  static SlotSpanMetadata* get_sentinel_slot_span_non_const();

  // Slot span state getters.
  PA_ALWAYS_INLINE bool is_active() const;
  PA_ALWAYS_INLINE bool is_full() const;
  PA_ALWAYS_INLINE bool is_empty() const;
  PA_ALWAYS_INLINE bool is_decommitted() const;
  PA_ALWAYS_INLINE bool freelist_is_sorted() const {
    return freelist_is_sorted_;
  }

 private:
  // sentinel_slot_span_ is used as a sentinel to indicate that there is no slot
  // span in the active list. We could use nullptr, but in that case we need to
  // add a null-check branch to the hot allocation path. We want to avoid that.
  //
  // Note, this declaration is kept in the header as opposed to an anonymous
  // namespace so the getter can be fully inlined.
  static const SlotSpanMetadata sentinel_slot_span_;
  // For the sentinel.
  inline constexpr SlotSpanMetadata() noexcept;

 public:
  // Note the matching Alloc() functions are in PartitionPage.
  PA_NOINLINE PA_COMPONENT_EXPORT(PARTITION_ALLOC) void FreeSlowPath(
      size_t number_of_freed);
  PA_ALWAYS_INLINE FreelistEntry* PopForAlloc(size_t size);
  PA_ALWAYS_INLINE void Free(uintptr_t ptr, PartitionRoot* root)
      PA_EXCLUSIVE_LOCKS_REQUIRED(PartitionRootLock(root));
  // Appends the passed freelist to the slot-span's freelist. Please note that
  // the function doesn't increment the tags of the passed freelist entries,
  // since FreeInline() did it already.
  PA_ALWAYS_INLINE void AppendFreeList(FreelistEntry* head,
                                       FreelistEntry* tail,
                                       size_t number_of_freed,
                                       PartitionRoot* root)
      PA_EXCLUSIVE_LOCKS_REQUIRED(PartitionRootLock(root));

  void Decommit(PartitionRoot* root);
  void DecommitIfPossible(PartitionRoot* root);

  // Sorts the freelist in ascending addresses order.
  void SortFreelist(const PartitionRoot* root);
  // Inserts the slot span into the empty ring, making space for the new slot
  // span, and potentially shrinking the ring.
  void RegisterEmpty();

  // The caller is responsible for ensuring that raw_size can be stored before
  // calling Set/GetRawSize.
  PA_ALWAYS_INLINE void SetRawSize(size_t raw_size);

  PA_ALWAYS_INLINE void SetFreelistHead(FreelistEntry* new_head);

  PA_ALWAYS_INLINE void Reset();

  PA_ALWAYS_INLINE void set_freelist_sorted() { freelist_is_sorted_ = true; }
};
#pragma pack(pop)
static_assert(sizeof(SlotSpanMetadata) <= kPageMetadataSize,
              "SlotSpanMetadata must fit into a Page Metadata slot.");

inline constexpr SlotSpanMetadata::SlotSpanMetadata() noexcept
    : num_allocated_slots(0u),
      num_unprovisioned_slots(0u),
      marked_full(0u),
      can_store_raw_size_(0u),
      freelist_is_sorted_(1u),
      in_empty_cache_(0u),
      empty_cache_index_(0u) {}

inline SlotSpanMetadata::SlotSpanMetadata(const SlotSpanMetadata&) = default;

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
// "Pack" the union so that common page metadata still fits within
// kPageMetadataSize. (SlotSpanMetadata is also "packed".)
#pragma pack(push, 1)
struct PartitionPageMetadata {
  union {
    SlotSpanMetadata slot_span_metadata;

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
  uint8_t slot_span_metadata_offset : kMaxSlotSpanMetadataBits;

  // |is_valid| tells whether the page is part of a slot span. If |false|,
  // |has_valid_span_after_this| tells whether it's an unused region in between
  // slot spans within the super page.
  // Note, |is_valid| has been added for clarity, but if we ever need to save
  // this bit, it can be inferred from:
  //   |!slot_span_metadata_offset && slot_span_metadata->bucket|.
  bool is_valid : 1;
  bool has_valid_span_after_this : 1;
  uint8_t unused;

  PA_ALWAYS_INLINE static PartitionPageMetadata* FromAddr(
      uintptr_t address,
      [[maybe_unused]] const PartitionRoot* root);
  PA_ALWAYS_INLINE static PartitionPageMetadata* FromAddr(
      uintptr_t address,
      [[maybe_unused]] std::ptrdiff_t offet);
};
#pragma pack(pop)
static_assert(sizeof(PartitionPageMetadata) == kPageMetadataSize,
              "PartitionPage must be able to fit in a metadata slot");

// Certain functions rely on PartitionPageMetadata being either SlotSpanMetadata
// or SubsequentPageMetadata, and therefore freely casting between each other.
// TODO(crbug.com/40940915) Stop ignoring the -Winvalid-offsetof warning.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert(offsetof(PartitionPageMetadata, slot_span_metadata) == 0,
              "slot_span_metadata must be placed at the beginning of "
              "PartitionPageMetadata.");
static_assert(offsetof(PartitionPageMetadata, subsequent_page_metadata) == 0,
              "subsequent_page_metadata must be placed at the beginning of "
              "PartitionPageMetadata.");
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

PA_ALWAYS_INLINE PartitionPageMetadata* PartitionSuperPageToMetadataArea(
    uintptr_t super_page,
    [[maybe_unused]] std::ptrdiff_t offset) {
  // This can't be just any super page, but it has to be the first super page of
  // the reservation, as we assume here that the metadata is near its beginning.
  PA_DCHECK(
      ReservationOffsetTable::Get(super_page).IsReservationStart(super_page));
  PA_DCHECK(!(super_page & kSuperPageOffsetMask));
  uintptr_t address = PartitionSuperPageToMetadataPage(super_page, offset);
#if !PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  // The metadata area is exactly one system page (the guard page) into the
  // super page.
  PA_DCHECK(super_page + SystemPageSize() == address);
#endif
  return reinterpret_cast<PartitionPageMetadata*>(address);
}

PA_ALWAYS_INLINE PartitionPageMetadata* PartitionSuperPageToMetadataArea(
    uintptr_t super_page,
    [[maybe_unused]] const PartitionRoot* root) {
  return PartitionSuperPageToMetadataArea(super_page, GetMetadataOffset(root));
}

PA_ALWAYS_INLINE const SubsequentPageMetadata* GetSubsequentPageMetadata(
    const PartitionPageMetadata* page_metadata) {
  return &(page_metadata + 1)->subsequent_page_metadata;
}

PA_ALWAYS_INLINE SubsequentPageMetadata* GetSubsequentPageMetadata(
    PartitionPageMetadata* page_metadata) {
  return &(page_metadata + 1)->subsequent_page_metadata;
}

PA_ALWAYS_INLINE PartitionSuperPageExtentEntry* PartitionSuperPageToExtent(
    uintptr_t super_page,
    [[maybe_unused]] std::ptrdiff_t offset) {
  // The very first entry of the metadata is the super page extent entry.
  return reinterpret_cast<PartitionSuperPageExtentEntry*>(
      PartitionSuperPageToMetadataArea(super_page, offset));
}

PA_ALWAYS_INLINE PartitionSuperPageExtentEntry* PartitionSuperPageToExtent(
    uintptr_t super_page,
    [[maybe_unused]] const PartitionRoot* root) {
  return PartitionSuperPageToExtent(super_page, GetMetadataOffset(root));
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
ReservedStateBitmapSize() {
  return 0ull;
}

PA_ALWAYS_INLINE uintptr_t SuperPagePayloadStartOffset() {
  return PartitionPageSize();
}

PA_ALWAYS_INLINE uintptr_t SuperPagePayloadBegin(uintptr_t super_page) {
  PA_DCHECK(!(super_page % kSuperPageAlignment));
  return super_page + SuperPagePayloadStartOffset();
}

PA_ALWAYS_INLINE uintptr_t SuperPagePayloadEndOffset() {
  return kSuperPageSize - PartitionPageSize();
}

PA_ALWAYS_INLINE uintptr_t SuperPagePayloadEnd(uintptr_t super_page) {
  PA_DCHECK(!(super_page % kSuperPageAlignment));
  return super_page + SuperPagePayloadEndOffset();
}

PA_ALWAYS_INLINE size_t SuperPagePayloadSize(uintptr_t super_page) {
  return SuperPagePayloadEnd(super_page) - SuperPagePayloadBegin(super_page);
}

PA_ALWAYS_INLINE PartitionSuperPageExtentEntry*
SlotSpanMetadata::ToSuperPageExtent() const {
  // SlotSpanMetadata and SuperPageExtentEntry are always in the same cage.
  // But the memory layout is different from GigaCage.
  uintptr_t super_page =
      reinterpret_cast<uintptr_t>(this) & SystemPageBaseMask();
  return reinterpret_cast<PartitionSuperPageExtentEntry*>(super_page);
}

// Returns whether the pointer lies within the super page's payload area (i.e.
// area devoted to slot spans). It doesn't check whether it's within a valid
// slot span. It merely ensures it doesn't fall in a meta-data region that would
// surely never contain user data.
PA_ALWAYS_INLINE bool IsWithinSuperPagePayload(uintptr_t address) {
  uintptr_t super_page = address & kSuperPageBaseMask;
  uintptr_t payload_start = SuperPagePayloadBegin(super_page);
  uintptr_t payload_end = SuperPagePayloadEnd(super_page);
  return address >= payload_start && address < payload_end;
}

// Converts from an address inside a super page into a pointer to the
// PartitionPageMetadata object (within super pages's metadata) that describes
// the partition page where |address| is located. |address| doesn't have to be
// located within a valid (i.e. allocated) slot span, but must be within the
// super page's payload area (i.e. area devoted to slot spans).
//
// While it is generally valid for |ptr| to be in the middle of an allocation,
// care has to be taken with direct maps that span multiple super pages. This
// function's behavior is undefined if |ptr| lies in a subsequent super page.
PA_ALWAYS_INLINE PartitionPageMetadata* PartitionPageMetadata::FromAddr(
    uintptr_t address,
    [[maybe_unused]] const PartitionRoot* root) {
  return PartitionPageMetadata::FromAddr(address, GetMetadataOffset(root));
}

PA_ALWAYS_INLINE PartitionPageMetadata* PartitionPageMetadata::FromAddr(
    uintptr_t address,
    [[maybe_unused]] std::ptrdiff_t metadata_offset) {
  uintptr_t super_page = address & kSuperPageBaseMask;

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_DCHECK(
      ReservationOffsetTable::Get(super_page).IsReservationStart(super_page));
  PA_DCHECK(IsWithinSuperPagePayload(address));
#endif

  uintptr_t partition_page_index =
      (address & kSuperPageOffsetMask) >> PartitionPageShift();
  // Index 0 is invalid because it is the super page extent metadata and the
  // last index is invalid because the whole PartitionPage is set as guard
  // pages. This repeats part of the payload PA_DCHECK above, which also checks
  // for other exclusions.
  PA_DCHECK(partition_page_index);
  PA_DCHECK(partition_page_index < NumPartitionPagesPerSuperPage() - 1);
  return PartitionSuperPageToMetadataArea(super_page, metadata_offset) +
         partition_page_index;
}

// Converts from a pointer to the SlotSpanMetadata object (within a super
// pages's metadata) into a pointer to the beginning of the slot span. This
// works on direct maps too.
PA_ALWAYS_INLINE SlotSpanStart
SlotSpanMetadata::ToSlotSpanStart(const SlotSpanMetadata* slot_span,
                                  [[maybe_unused]] const PartitionRoot* root) {
  return ToSlotSpanStart(slot_span, GetMetadataOffset(root));
}

PA_ALWAYS_INLINE SlotSpanStart
SlotSpanMetadata::ToSlotSpanStart(const SlotSpanMetadata* slot_span,
                                  [[maybe_unused]] std::ptrdiff_t offset) {
  uintptr_t slot_span_addr = reinterpret_cast<uintptr_t>(slot_span);
#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  uintptr_t partition_page_index =
      (slot_span_addr & SystemPageOffsetMask()) >> kPageMetadataShift;
  uintptr_t super_page_base =
      PartitionMetadataPageToSuperPage(slot_span_addr, offset) &
      kSuperPageBaseMask;
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_DCHECK(PartitionAddressSpace::IsInMetadataRegion(slot_span_addr) ||
            super_page_base == (slot_span_addr & kSuperPageBaseMask));
#endif
  return SlotSpanStart(super_page_base +
                       (partition_page_index << PartitionPageShift()));
#else
  uintptr_t super_page_offset = (slot_span_addr & kSuperPageOffsetMask);

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
  uintptr_t super_page_base = slot_span_addr & kSuperPageBaseMask;
  return SlotSpanStart(super_page_base +
                       (partition_page_index << PartitionPageShift()));
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
}

// Converts an address inside a slot span into a pointer to the SlotSpanMetadata
// object (within super pages's metadata) that describes the slot span
// containing that slot.
//
// CAUTION! For direct-mapped allocation, |address| has to be within the first
// partition page.
PA_ALWAYS_INLINE SlotSpanMetadata* SlotSpanMetadata::FromAddr(
    uintptr_t address) {
#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  std::ptrdiff_t metadata_offset =
      PartitionAddressSpace::MetadataOffset(GetPool(address));
#else
  constexpr std::ptrdiff_t metadata_offset = 0;
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  return FromAddr(address, metadata_offset);
}

PA_ALWAYS_INLINE SlotSpanMetadata* SlotSpanMetadata::FromAddr(
    uintptr_t address,
    [[maybe_unused]] const PartitionRoot* root) {
  auto* page_metadata = PartitionPageMetadata::FromAddr(address, root);
  PA_DCHECK(page_metadata->is_valid);
  // Partition pages in the same slot span share the same SlotSpanMetadata
  // object (located in the first PartitionPageMetadata object of that span).
  // Adjust for that.
  page_metadata -= page_metadata->slot_span_metadata_offset;
  PA_DCHECK(page_metadata->is_valid);
  PA_DCHECK(!page_metadata->slot_span_metadata_offset);
  auto* slot_span = &page_metadata->slot_span_metadata;
  PA_DCHECK(DeducedRootIsValid(slot_span));
  // For direct map, if |address| doesn't point within the first partition page,
  // |slot_span_metadata_offset| will be 0, |page_metadata| won't get shifted,
  // leaving |slot_size| at 0.
  PA_DCHECK(slot_span->bucket->slot_size);
  return slot_span;
}

PA_ALWAYS_INLINE SlotSpanMetadata* SlotSpanMetadata::FromAddr(
    uintptr_t address,
    [[maybe_unused]] std::ptrdiff_t offset) {
  auto* page_metadata = PartitionPageMetadata::FromAddr(address, offset);
  PA_DCHECK(page_metadata->is_valid);
  // Partition pages in the same slot span share the same SlotSpanMetadata
  // object (located in the first PartitionPageMetadata object of that span).
  // Adjust for that.
  page_metadata -= page_metadata->slot_span_metadata_offset;
  PA_DCHECK(page_metadata->is_valid);
  PA_DCHECK(!page_metadata->slot_span_metadata_offset);
  auto* slot_span = &page_metadata->slot_span_metadata;
  PA_DCHECK(DeducedRootIsValid(slot_span));
  // For direct map, if |address| doesn't point within the first partition page,
  // |slot_span_metadata_offset| will be 0, |page_metadata| won't get shifted,
  // leaving |slot_size| at 0.
  PA_DCHECK(slot_span->bucket->slot_size);
  return slot_span;
}

// Like |FromAddr|, but asserts that |slot_start| indeed points to the
// beginning of a slot. It doesn't check if the slot is actually allocated.
//
// This works on direct maps too.
PA_ALWAYS_INLINE SlotSpanMetadata* SlotSpanMetadata::FromSlotStart(
    UntaggedSlotStart slot_start) {
#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  std::ptrdiff_t metadata_offset =
      PartitionAddressSpace::MetadataOffset(GetPool(slot_start.value()));
#else
  constexpr std::ptrdiff_t metadata_offset = 0;
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  return FromSlotStart(slot_start, metadata_offset);
}

PA_ALWAYS_INLINE SlotSpanMetadata* SlotSpanMetadata::FromSlotStart(
    UntaggedSlotStart slot_start,
    [[maybe_unused]] const PartitionRoot* root) {
  return FromSlotStart(slot_start, GetMetadataOffset(root));
}

PA_ALWAYS_INLINE SlotSpanMetadata* SlotSpanMetadata::FromSlotStart(
    UntaggedSlotStart slot_start,
    [[maybe_unused]] std::ptrdiff_t offset) {
  auto* slot_span = FromAddr(slot_start.value(), offset);

  // Checks that the pointer is a multiple of slot size.
  PA_DCHECK(!(ToSlotSpanStart(slot_span, offset).offset(slot_start) %
              static_cast<ptrdiff_t>(slot_span->bucket->slot_size)));
  return slot_span;
}

// Like |FromAddr|, but asserts that |address| indeed points within an object.
// It doesn't check if the object is actually allocated.
//
// CAUTION! For direct-mapped allocation, |address| has to be within the first
// partition page.
PA_ALWAYS_INLINE SlotSpanMetadata* SlotSpanMetadata::FromObjectInnerAddr(
    uintptr_t address,
    [[maybe_unused]] const PartitionRoot* root) {
  return FromObjectInnerAddr(address, GetMetadataOffset(root));
}

PA_ALWAYS_INLINE SlotSpanMetadata* SlotSpanMetadata::FromObjectInnerAddr(
    uintptr_t address,
    [[maybe_unused]] std::ptrdiff_t offset) {
  auto* slot_span = FromAddr(address, offset);
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  // Checks that the address is within the expected object boundaries.
  ptrdiff_t shift_from_slot_start =
      ToSlotSpanStart(slot_span, offset).offset(address) %
      static_cast<ptrdiff_t>(slot_span->bucket->slot_size);
  DCheckIsValidShiftFromSlotStart(
      slot_span, static_cast<uintptr_t>(shift_from_slot_start));
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
  return slot_span;
}

PA_ALWAYS_INLINE SlotSpanMetadata* SlotSpanMetadata::FromObjectInnerPtr(
    const void* ptr,
    [[maybe_unused]] const PartitionRoot* root) {
  return FromObjectInnerAddr(ObjectInnerPtr2Addr(ptr), root);
}

PA_ALWAYS_INLINE SlotSpanMetadata* SlotSpanMetadata::FromObjectInnerPtr(
    const void* ptr,
    [[maybe_unused]] std::ptrdiff_t offset) {
  return FromObjectInnerAddr(ObjectInnerPtr2Addr(ptr), offset);
}

PA_ALWAYS_INLINE void SlotSpanMetadata::SetRawSize(size_t raw_size) {
  PA_DCHECK(CanStoreRawSize());
  auto* subsequent_page_metadata =
      GetSubsequentPageMetadata(reinterpret_cast<PartitionPageMetadata*>(this));
  subsequent_page_metadata->raw_size = raw_size;
}

PA_ALWAYS_INLINE size_t SlotSpanMetadata::GetRawSize() const {
  PA_DCHECK(CanStoreRawSize());
  const auto* subsequent_page_metadata = GetSubsequentPageMetadata(
      reinterpret_cast<const PartitionPageMetadata*>(this));
  return subsequent_page_metadata->raw_size;
}

PA_ALWAYS_INLINE void SlotSpanMetadata::SetFreelistHead(
    FreelistEntry* new_head) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  // |this| is in the metadata region, hence isn't MTE-tagged. Untag |new_head|
  // as well.
  uintptr_t new_head_untagged = UntagPtr(new_head);
  const PartitionRoot* root = ToSuperPageExtent()->root;
  PA_DCHECK(!new_head ||
            (PartitionMetadataPageToSuperPage(reinterpret_cast<uintptr_t>(this),
                                              GetMetadataOffset(root)) &
             kSuperPageBaseMask) == (new_head_untagged & kSuperPageBaseMask));
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
  freelist_head = new_head;
  // Inserted something new in the freelist, assume that it is not sorted
  // anymore.
  freelist_is_sorted_ = false;
}

PA_ALWAYS_INLINE FreelistEntry* SlotSpanMetadata::PopForAlloc(size_t size) {
  // Not using bucket->slot_size directly as the compiler doesn't know that
  // |bucket->slot_size| is the same as |size|.
  PA_DCHECK(size == bucket->slot_size);
  FreelistEntry* result = freelist_head;
  // Not setting freelist_is_sorted_ to false since this doesn't destroy
  // ordering.
  freelist_head = freelist_head->GetNext(size);

  num_allocated_slots++;
  return result;
}

PA_ALWAYS_INLINE void SlotSpanMetadata::Free(uintptr_t slot_start,
                                             PartitionRoot* root)
    // PartitionRootLock() is not defined inside partition_page.h, but
    // static analysis doesn't require the implementation.
    PA_EXCLUSIVE_LOCKS_REQUIRED(PartitionRootLock(root)) {
  DCheckRootLockIsAcquired(root);
  auto* entry =
      UntaggedSlotStart::Unchecked(slot_start).Tag().ToObject<FreelistEntry>();
  // Catches an immediate double free.
  PA_CHECK(entry != freelist_head);

  // Look for double free one level deeper in debug.
  PA_DCHECK(!freelist_head ||
            entry != freelist_head->GetNext(bucket->slot_size));
  entry->SetNext(freelist_head);
  SetFreelistHead(entry);
  // A best effort double-free check. Works only on empty slot spans.
  PA_CHECK(num_allocated_slots);
  --num_allocated_slots;
  // If the span is marked full, or became empty, take the slow path to update
  // internal state.
  if (marked_full || num_allocated_slots == 0) [[unlikely]] {
    FreeSlowPath(1);
  } else {
    // All single-slot allocations must go through the slow path to
    // correctly update the raw size.
    PA_DCHECK(!CanStoreRawSize());
  }
}

PA_ALWAYS_INLINE void SlotSpanMetadata::AppendFreeList(FreelistEntry* head,
                                                       FreelistEntry* tail,
                                                       size_t number_of_freed,
                                                       PartitionRoot* root)
    PA_EXCLUSIVE_LOCKS_REQUIRED(PartitionRootLock(root)) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  DCheckRootLockIsAcquired(root);
  PA_DCHECK(!(tail->GetNext(bucket->slot_size)));
  PA_DCHECK(number_of_freed);
  PA_DCHECK(num_allocated_slots);
  if (CanStoreRawSize()) {
    PA_DCHECK(number_of_freed == 1);
  }
  {
    size_t number_of_entries = 0;
    for (auto* entry = head; entry;
         entry = entry->GetNext(bucket->slot_size), ++number_of_entries) {
      uintptr_t untagged_entry = UntagPtr(entry);
      // Check that all entries belong to this slot span.
      PA_DCHECK(ToSlotSpanStart(this, root).value() <= untagged_entry);
      PA_DCHECK(untagged_entry < ToSlotSpanStart(this, root).value() +
                                     bucket->get_bytes_per_span());
    }
    PA_DCHECK(number_of_entries == number_of_freed);
  }
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

  tail->SetNext(freelist_head);
  SetFreelistHead(head);
  PA_DCHECK(num_allocated_slots >= number_of_freed);
  num_allocated_slots -= number_of_freed;
  // If the span is marked full, or became empty, take the slow path to update
  // internal state.
  if (marked_full || num_allocated_slots == 0) [[unlikely]] {
    FreeSlowPath(number_of_freed);
  } else {
    // All single-slot allocations must go through the slow path to
    // correctly update the raw size.
    PA_DCHECK(!CanStoreRawSize());
  }
}

PA_ALWAYS_INLINE bool SlotSpanMetadata::is_active() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  bool ret =
      (num_allocated_slots > 0 && (freelist_head || num_unprovisioned_slots));
  if (ret) {
    PA_DCHECK(!marked_full);
    PA_DCHECK(num_allocated_slots < bucket->get_slots_per_span());
  }
  return ret;
}

PA_ALWAYS_INLINE bool SlotSpanMetadata::is_full() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  bool ret = (num_allocated_slots == bucket->get_slots_per_span());
  if (ret) {
    PA_DCHECK(!freelist_head);
    PA_DCHECK(!num_unprovisioned_slots);
    // May or may not be marked full, so don't check for that.
  }
  return ret;
}

PA_ALWAYS_INLINE bool SlotSpanMetadata::is_empty() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  bool ret = (!num_allocated_slots && freelist_head);
  if (ret) {
    PA_DCHECK(!marked_full);
  }
  return ret;
}

PA_ALWAYS_INLINE bool SlotSpanMetadata::is_decommitted() const {
  PA_DCHECK(this != get_sentinel_slot_span());
  bool ret = (!num_allocated_slots && !freelist_head);
  if (ret) {
    PA_DCHECK(!marked_full);
    PA_DCHECK(!num_unprovisioned_slots);
    PA_DCHECK(!in_empty_cache_);
  }
  return ret;
}

PA_ALWAYS_INLINE void SlotSpanMetadata::Reset() {
  PA_DCHECK(is_decommitted());

  size_t num_slots_per_span = bucket->get_slots_per_span();
  PA_DCHECK(num_slots_per_span <= kMaxSlotsPerSlotSpan);
  num_unprovisioned_slots = static_cast<uint16_t>(num_slots_per_span);
  PA_DCHECK(num_unprovisioned_slots);

  ToSuperPageExtent()->IncrementNumberOfNonemptySlotSpans();

  next_slot_span = nullptr;
}

// Iterates over all slot spans in a super-page. |Callback| must return true if
// early return is needed.
template <typename Callback>
void IterateSlotSpans(uintptr_t super_page,
                      [[maybe_unused]] const PartitionRoot* root,
                      Callback callback) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_DCHECK(!(super_page % kSuperPageAlignment));
  auto* extent_entry = PartitionSuperPageToExtent(super_page, root);
  DCheckRootLockIsAcquired(extent_entry->root);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

  auto* const first_page_metadata =
      PartitionPageMetadata::FromAddr(SuperPagePayloadBegin(super_page), root);
  auto* const last_page_metadata = PartitionPageMetadata::FromAddr(
      SuperPagePayloadEnd(super_page) - PartitionPageSize(), root);
  PartitionPageMetadata* page_metadata = nullptr;
  SlotSpanMetadata* slot_span = nullptr;
  for (page_metadata = first_page_metadata;
       page_metadata <= last_page_metadata;) {
    PA_DCHECK(!page_metadata
                   ->slot_span_metadata_offset);  // Ensure slot span beginning.
    if (!page_metadata->is_valid) {
      if (page_metadata->has_valid_span_after_this) {
        // page_metadata doesn't represent a valid slot span, but there is
        // another one somewhere after this. Keep iterating to find it.
        ++page_metadata;
        continue;
      }
      // There are currently no valid spans from here on. No need to iterate
      // the rest of the super page_metadata.
      break;
    }
    slot_span = &page_metadata->slot_span_metadata;
    if (callback(slot_span)) {
      return;
    }
    page_metadata += slot_span->bucket->get_pages_per_slot_span();
  }
  // Each super page must have at least one valid slot span.
  PA_DCHECK(page_metadata > first_page_metadata);
  // Just a quick check that the search ended at a valid slot span and there
  // was no unnecessary iteration over gaps afterwards.
  PA_DCHECK(page_metadata ==
            reinterpret_cast<PartitionPageMetadata*>(slot_span) +
                slot_span->bucket->get_pages_per_slot_span());
}

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PARTITION_PAGE_H_
