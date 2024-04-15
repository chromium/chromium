// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_POOL_OFFSET_FREELIST_H_
#define PARTITION_ALLOC_POOL_OFFSET_FREELIST_H_

#include <cstddef>
#include <cstdint>

#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc-inl.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/tagging.h"

namespace partition_alloc::internal {

// This implementation of PartitionAlloc's freelist uses pool offsets
// rather than naked pointers. This is intended to prevent usage of
// freelist pointers to easily jump around to freed slots.
class PoolOffsetFreelistEntry {
  using PoolInfo = PartitionAddressSpace::PoolInfo;

  constexpr static uintptr_t kNullptrOffset = 0;

  constexpr explicit PoolOffsetFreelistEntry(std::nullptr_t)
      : next_(kNullptrOffset)
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
        ,
        shadow_(~next_)
#endif  // PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
  {
  }
  explicit PoolOffsetFreelistEntry(PoolOffsetFreelistEntry* next)
      : next_(EncodeToOffset(next))
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
        ,
        shadow_(~next_)
#endif  // PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
  {
  }
  // For testing only.
  PoolOffsetFreelistEntry(void* next, bool make_shadow_match)
      : next_(EncodeToOffset(next))
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
        ,
        shadow_(make_shadow_match ? ~next_ : 12345)
#endif  // PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
  {
  }

 public:
  ~PoolOffsetFreelistEntry() = delete;

  PA_ALWAYS_INLINE static PoolOffsetFreelistEntry* EmplaceAndInitNull(
      void* slot_start_tagged) {
    auto* entry = new (slot_start_tagged) PoolOffsetFreelistEntry(nullptr);
    return entry;
  }

  PA_ALWAYS_INLINE static PoolOffsetFreelistEntry* EmplaceAndInitNull(
      uintptr_t slot_start) {
    return EmplaceAndInitNull(SlotStartAddr2Ptr(slot_start));
  }

  PA_ALWAYS_INLINE static PoolOffsetFreelistEntry* EmplaceAndInitForThreadCache(
      uintptr_t slot_start,
      PoolOffsetFreelistEntry* next) {
    auto* entry =
        new (SlotStartAddr2Ptr(slot_start)) PoolOffsetFreelistEntry(next);
    return entry;
  }

  PA_ALWAYS_INLINE static void EmplaceAndInitForTest(uintptr_t slot_start,
                                                     void* next,
                                                     bool make_shadow_match) {
    new (SlotStartAddr2Ptr(slot_start))
        PoolOffsetFreelistEntry(next, make_shadow_match);
  }

  void CorruptNextForTesting(uintptr_t v) {
    // TODO(crbug.com/1461983): Make this do something useful.
    next_ += 1ull << 31;
  }

  template <bool crash_on_corruption>
  PA_ALWAYS_INLINE PoolOffsetFreelistEntry* GetNextForThreadCache(
      size_t slot_size) const {
    return GetNextInternal<crash_on_corruption, /*for_thread_cache=*/true>(
        slot_size);
  }

  PA_ALWAYS_INLINE PoolOffsetFreelistEntry* GetNext(size_t slot_size) const {
    return GetNextInternal<true, /*for_thread_cache=*/false>(slot_size);
  }

  PA_NOINLINE void CheckFreeList(size_t slot_size) const {
    for (auto* entry = this; entry; entry = entry->GetNext(slot_size)) {
      // `GetNext()` calls `IsWellFormed()`.
    }
  }

  PA_NOINLINE void CheckFreeListForThreadCache(size_t slot_size) const {
    for (auto* entry = this; entry;
         entry = entry->GetNextForThreadCache<true>(slot_size)) {
      // `GetNextForThreadCache()` calls `IsWellFormed()`.
    }
  }

  PA_ALWAYS_INLINE void SetNext(PoolOffsetFreelistEntry* entry) {
    // SetNext() is either called on the freelist head, when provisioning new
    // slots, or when GetNext() has been called before, no need to pass the
    // size.
#if BUILDFLAG(PA_DCHECK_IS_ON)
    // Regular freelists always point to an entry within the same super page.
    //
    // This is most likely a PartitionAlloc bug if this triggers.
    if (PA_UNLIKELY(entry &&
                    (SlotStartPtr2Addr(this) & kSuperPageBaseMask) !=
                        (SlotStartPtr2Addr(entry) & kSuperPageBaseMask))) {
      FreelistCorruptionDetected(0);
    }
#endif  // BUILDFLAG(PA_DCHECK_IS_ON)

    next_ = EncodeToOffset(entry);
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
    shadow_ = ~next_;
#endif
  }

  // Zeroes out |this| before returning the slot. The pointer to this memory
  // will be returned to the user (caller of Alloc()), thus can't have internal
  // data.
  PA_ALWAYS_INLINE uintptr_t ClearForAllocation() {
    next_ = uintptr_t{0};
    shadow_ = uintptr_t{0};
    return SlotStartPtr2Addr(this);
  }

  PA_ALWAYS_INLINE constexpr bool IsEncodedNextPtrZero() const {
    return next_ == kNullptrOffset;
  }

 private:
  // Determines the containing pool of `ptr` and returns `ptr`
  // represented as a tagged offset into that pool. `ptr` can be `nullptr`.
  PA_ALWAYS_INLINE static uintptr_t EncodeToOffset(void* ptr) {
    if (!ptr) {
      return kNullptrOffset;
    }
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    PoolInfo pool_info = PartitionAddressSpace::GetPoolInfo(addr);
    // Save a MTE tag as well as an offset.
    uintptr_t tagged_offset = addr & (kPtrTagMask | ~pool_info.base_mask);
    PA_DCHECK(tagged_offset == (pool_info.offset | (addr & kPtrTagMask)));
    return tagged_offset;
  }

  // Given `pool_info`, decodes a `tagged_offset` into a tagged pointer.
  // `tagged_offset` cannot be `kNullptrOffset`, please call
  // `IsEncodedNextPtrZero()` before.
  PA_ALWAYS_INLINE static PoolOffsetFreelistEntry* DecodeFromOffset(
      uintptr_t tagged_offset,
      PoolInfo& pool_info) {
    PA_DCHECK(tagged_offset != kNullptrOffset);
    // We assume `tagged_offset` contains a proper MTE tag.
    return reinterpret_cast<PoolOffsetFreelistEntry*>(pool_info.base |
                                                      tagged_offset);
  }

  template <bool crash_on_corruption, bool for_thread_cache>
  PA_ALWAYS_INLINE PoolOffsetFreelistEntry* GetNextInternal(
      size_t slot_size) const {
    // GetNext() can be called on discarded memory, in which case
    // |encoded_next_| is 0, and none of the checks apply. Don't prefetch
    // nullptr either.
    if (IsEncodedNextPtrZero()) {
      return nullptr;
    }

    PoolInfo pool_info = GetPoolInfo(reinterpret_cast<uintptr_t>(this));
    // We assume `(next_ & pool_info.base_mask) == 0` here.
    // This assumption is checked in `IsWellFormed()` later.
    auto* ret = DecodeFromOffset(next_, pool_info);
    if (PA_UNLIKELY(!IsWellFormed<for_thread_cache>(pool_info, this, ret))) {
      if constexpr (crash_on_corruption) {
        PA_DEBUG_DATA_ON_STACK("first", static_cast<size_t>(next_));
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
        PA_DEBUG_DATA_ON_STACK("second", static_cast<size_t>(shadow_));
#endif
        FreelistCorruptionDetected(slot_size);
      }
      return nullptr;
    }
    PA_PREFETCH(ret);
    return ret;
  }

  template <bool for_thread_cache>
  PA_ALWAYS_INLINE static bool IsWellFormed(
      const PoolInfo& pool_info,
      const PoolOffsetFreelistEntry* here,
      const PoolOffsetFreelistEntry* next) {
    // Don't allow the freelist to be blindly followed to any location.
    // Checks following constraints:
    // - `shadow_` must match an inversion of `next_` (if present).
    // - next must not have bits in the pool base mask except a MTE tag.
    // - here and next must belong to the same superpage, unless this is in the
    //   thread cache (they even always belong to the same slot span).
    // - next cannot point inside the metadata area.

    // TODO(crbug.com/1461983): Add support for freeslot bitmaps.
    static_assert(!BUILDFLAG(USE_FREESLOT_BITMAP),
                  "USE_FREESLOT_BITMAP not supported");

    const uintptr_t here_address = SlotStartPtr2Addr(here);
    const uintptr_t next_address = SlotStartPtr2Addr(next);

    const bool no_bit_in_pool_base_mask = !(here->next_ & pool_info.base_mask);

    const bool not_in_metadata =
        (next_address & kSuperPageOffsetMask) >= PartitionPageSize();

#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
    bool shadow_ptr_ok = (~here->next_) == here->shadow_;
#else
    constexpr bool shadow_ptr_ok = true;
#endif

    if constexpr (for_thread_cache) {
      return no_bit_in_pool_base_mask & shadow_ptr_ok & not_in_metadata;
    }

    const bool same_super_page = (here_address & kSuperPageBaseMask) ==
                                 (next_address & kSuperPageBaseMask);
    return no_bit_in_pool_base_mask & shadow_ptr_ok & same_super_page &
           not_in_metadata;
  }

  // Expresses the next entry in the freelist as an offset in the
  // same pool as `this`, except when `kNullptrOffset`, which (as an invalid
  // pool offset) serves as a sentinel value.
  uintptr_t next_ = uintptr_t{0};
  // This is intended to detect unintentional corruptions of the freelist.
  // These can happen due to a Use-after-Free, or overflow of the previous
  // allocation in the slot span.
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
  uintptr_t shadow_;
#endif
};

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_POOL_OFFSET_FREELIST_H_
