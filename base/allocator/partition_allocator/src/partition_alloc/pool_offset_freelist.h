// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POOL_OFFSET_FREELIST_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POOL_OFFSET_FREELIST_H_

#include <cstddef>
#include <cstdint>

#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc-inl.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"

namespace partition_alloc::internal {

// This implementation of PartitionAlloc's freelist uses pool offsets
// rather than naked pointers. This is intended to prevent usage of
// freelist pointers to easily jump around to freed slots.
class PoolOffsetFreelistEntry {
 private:
  constexpr explicit PoolOffsetFreelistEntry(std::nullptr_t) {}

  explicit PoolOffsetFreelistEntry(PoolOffsetFreelistEntry* next)
      : next_(PoolOffset(reinterpret_cast<uintptr_t>(next))) {}

  // For testing only.
  PoolOffsetFreelistEntry(void* next, bool make_shadow_match)
      : next_(PoolOffset(reinterpret_cast<uintptr_t>(next))) {}

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
    next_ = PoolOffset(reinterpret_cast<uintptr_t>(entry));
  }

  PA_ALWAYS_INLINE uintptr_t ClearForAllocation() {
    next_ = uintptr_t{0};
    return SlotStartPtr2Addr(this);
  }

  PA_ALWAYS_INLINE constexpr bool IsEncodedNextPtrZero() const {
    return !next_;
  }

 private:
  // Determines the containing pool of `addr` and returns `addr`
  // represented as an offset into that pool.
  PA_ALWAYS_INLINE static uintptr_t PoolOffset(uintptr_t addr) {
    return addr ? PartitionAddressSpace::GetPoolInfo(addr).offset : addr;
  }

  template <bool crash_on_corruption, bool for_thread_cache>
  PA_ALWAYS_INLINE PoolOffsetFreelistEntry* GetNextInternal(
      size_t slot_size) const {
    if (IsEncodedNextPtrZero()) {
      return nullptr;
    }

    auto* ret = reinterpret_cast<PoolOffsetFreelistEntry*>(
        GetPoolInfo(reinterpret_cast<uintptr_t>(this)).base + next_);
    if (PA_UNLIKELY(!IsWellFormed<for_thread_cache>(this, ret))) {
      if constexpr (crash_on_corruption) {
        PA_DEBUG_DATA_ON_STACK("first", static_cast<size_t>(next_));
        FreelistCorruptionDetected(slot_size);
      }
      return nullptr;
    }
    PA_PREFETCH(ret);
    return ret;
  }

  // TODO(crbug.com/1461983): Add support for freelist shadow entries
  // (and freeslot bitmaps).
  template <bool for_thread_cache>
  PA_ALWAYS_INLINE static bool IsWellFormed(
      const PoolOffsetFreelistEntry* here,
      const PoolOffsetFreelistEntry* next) {
    const uintptr_t here_address = SlotStartPtr2Addr(here);
    const uintptr_t next_address = SlotStartPtr2Addr(next);

    const bool not_in_metadata =
        (next_address & kSuperPageOffsetMask) >= PartitionPageSize();
    if constexpr (for_thread_cache) {
      return not_in_metadata;
    }
    const bool same_super_page = (here_address & kSuperPageBaseMask) ==
                                 (next_address & kSuperPageBaseMask);
    return same_super_page && not_in_metadata;
  }

  // Expresses the next entry in the freelist as an offset in the
  // same pool as `this`, except when 0, which (as an invalid pool
  // offset) serves as a sentinel value.
  uintptr_t next_ = uintptr_t{0};
};

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POOL_OFFSET_FREELIST_H_
