// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef PARTITION_ALLOC_PARTITION_PAGE_H_
#define PARTITION_ALLOC_PARTITION_PAGE_H_

#include <cstddef>
#include <cstdint>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_freelist_entry.h"
#include "partition_alloc/partition_page_constants.h"
#include "partition_alloc/partition_superpage_extent_entry.h"

namespace partition_alloc::internal {

struct PartitionBucket;

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
  PA_ALWAYS_INLINE bool CanStoreRawSize() const;

  // Returns the total size of the slots that are currently provisioned.
  PA_ALWAYS_INLINE size_t GetProvisionedSize() const;

  // Return the number of entries in the freelist.
  PA_ALWAYS_INLINE size_t GetFreelistLength() const;

  PA_ALWAYS_INLINE bool in_empty_cache() const;

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

  PA_ALWAYS_INLINE FreelistEntry* get_freelist_head() const;

  // Returns size of the region used within a slot. The used region comprises
  // of actual allocated data, extras and possibly empty space in the middle.
  PA_ALWAYS_INLINE size_t GetUtilizedSlotSize() const;

  // This includes padding due to rounding done at allocation; we don't know the
  // requested size at deallocation, so we use this in both places.
  PA_ALWAYS_INLINE size_t GetSlotSizeForBookkeeping() const;

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
  PA_ALWAYS_INLINE bool freelist_is_sorted() const;

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

  PA_ALWAYS_INLINE void set_freelist_sorted();
};
#pragma pack(pop)
static_assert(sizeof(SlotSpanMetadata) <= kPageMetadataSize,
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


}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PARTITION_PAGE_H_
