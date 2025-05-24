// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_FREELIST_ENTRY_H_
#define PARTITION_ALLOC_PARTITION_FREELIST_ENTRY_H_

#include <utility>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_constants.h"

// Pool-offset encoding has better security characteristics, but requires
// contiguous pool hence limited to 64-bit systems.
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#include "partition_alloc/pool_offset_freelist.h"
#else
#include "partition_alloc/encoded_next_freelist.h"
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

namespace partition_alloc::internal {

// Freelist entries are encoded for security reasons. See
// //base/allocator/partition_allocator/PartitionAlloc.md
// and |Transform()| for the rationale and mechanism, respectively.
class FreelistEntry {
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  using EncodedPtr = EncodedPoolOffset;
#else
  using EncodedPtr = EncodedFreelistPtr;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

  constexpr explicit FreelistEntry(std::nullptr_t)
      : encoded_next_(EncodedPtr(nullptr))
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
        ,
        shadow_(encoded_next_.Inverted())
#endif
  {
  }
  template <typename... Args>
  explicit FreelistEntry(FreelistEntry* next, Args&&... args)
      : encoded_next_(EncodedPtr(next, std::forward<Args>(args)...))
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
        ,
        shadow_(encoded_next_.Inverted())
#endif
  {
  }
  // For testing only.
  template <typename... Args>
  FreelistEntry(void* next, Args&&... args, bool make_shadow_match)
      : encoded_next_(EncodedPtr(next, std::forward<Args>(args)...))
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
        ,
        shadow_(make_shadow_match ? encoded_next_.Inverted() : 12345)
#endif
  {
  }

 public:
  ~FreelistEntry() = delete;

  // Emplaces the freelist entry at the beginning of the given slot span, and
  // initializes it as null-terminated.
  PA_ALWAYS_INLINE static FreelistEntry* EmplaceAndInitNull(
      void* slot_start_tagged) {
    // |slot_start_tagged| is MTE-tagged.
    auto* entry = new (slot_start_tagged) FreelistEntry(nullptr);
    return entry;
  }
  PA_ALWAYS_INLINE static FreelistEntry* EmplaceAndInitNull(
      uintptr_t slot_start) {
    return EmplaceAndInitNull(SlotStartAddr2Ptr(slot_start));
  }

  // Emplaces the freelist entry at the beginning of the given slot span, and
  // initializes it with the given |next| pointer, but encoded.
  //
  // This freelist is built for the purpose of thread-cache. This means that we
  // can't perform a check that this and the next pointer belong to the same
  // super page, as thread-cache spans may chain slots across super pages.
  template <typename... Args>
  PA_ALWAYS_INLINE static FreelistEntry* EmplaceAndInitForThreadCache(
      uintptr_t slot_start,
      FreelistEntry* next,
      Args&&... args) {
    auto* entry = new (SlotStartAddr2Ptr(slot_start))
        FreelistEntry(next, std::forward<Args>(args)...);
    return entry;
  }

  // Emplaces the freelist entry at the beginning of the given slot span, and
  // initializes it with the given |next| pointer.
  //
  // This is for testing purposes only! |make_shadow_match| allows you to choose
  // if the shadow matches the next pointer properly or is trash.
  template <typename... Args>
  PA_ALWAYS_INLINE static void EmplaceAndInitForTest(uintptr_t slot_start,
                                                     void* next,
                                                     Args&&... args,
                                                     bool make_shadow_match) {
    new (SlotStartAddr2Ptr(slot_start))
        FreelistEntry(next, std::forward<Args>(args)..., make_shadow_match);
  }

  void CorruptNextForTesting(uintptr_t v) {
    // We just need a value that can never be a valid value here.
    encoded_next_.Override(EncodedPtr::Transform(v));
  }

  // Puts `slot_size` on the stack before crashing in case of memory
  // corruption. Meant to be used to report the failed allocation size.
  template <typename... Args>
  PA_ALWAYS_INLINE FreelistEntry* GetNextForThreadCache(size_t slot_size,
                                                        Args&&... args) const {
    return GetNextInternal</*for_thread_cache=*/true>(
        slot_size, std::forward<Args>(args)...);
  }
  template <typename... Args>
  PA_ALWAYS_INLINE FreelistEntry* GetNext(size_t slot_size,
                                          Args&&... args) const {
    return GetNextInternal</*for_thread_cache=*/false>(
        slot_size, std::forward<Args>(args)...);
  }

  PA_NOINLINE void CheckFreeList(size_t slot_size) const {
    for (auto* entry = this; entry; entry = entry->GetNext(slot_size)) {
      // `GetNext()` calls `IsWellFormed()`.
    }
  }

  PA_NOINLINE void CheckFreeListForThreadCache(size_t slot_size) const {
    for (auto* entry = this; entry;
         entry = entry->GetNextForThreadCache(slot_size)) {
      // `GetNextForThreadCache()` calls `IsWellFormed()`.
    }
  }

  template <typename... Args>
  PA_ALWAYS_INLINE void SetNext(FreelistEntry* entry, Args&&... args) {
    // SetNext() is either called on the freelist head, when provisioning new
    // slots, or when GetNext() has been called before, no need to pass the
    // size.
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
    // Regular freelists always point to an entry within the same super page.
    //
    // This is most likely a PartitionAlloc bug if this triggers.
    if (entry && (SlotStartPtr2Addr(this) & kSuperPageBaseMask) !=
                     (SlotStartPtr2Addr(entry) & kSuperPageBaseMask))
        [[unlikely]] {
      FreelistCorruptionDetected(0);
    }
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

    encoded_next_ = EncodedPtr(entry, std::forward<Args>(args)...);
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
    shadow_ = encoded_next_.Inverted();
#endif
  }

  // Zeroes out |this| before returning the slot. The pointer to this memory
  // will be returned to the user (caller of Alloc()), thus can't have internal
  // data.
  PA_ALWAYS_INLINE uintptr_t ClearForAllocation() {
    encoded_next_.Override(0);
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
    shadow_ = 0;
#endif
    return SlotStartPtr2Addr(this);
  }

  PA_ALWAYS_INLINE constexpr bool IsEncodedNextPtrZero() const {
    return !encoded_next_;
  }

 private:
  template <bool for_thread_cache, typename... Args>
  PA_ALWAYS_INLINE FreelistEntry* GetNextInternal(size_t slot_size,
                                                  Args&&... args) const {
    // GetNext() can be called on discarded memory, in which case
    // |encoded_next_| is 0, and none of the checks apply. Don't prefetch
    // nullptr either.
    if (IsEncodedNextPtrZero()) {
      return nullptr;
    }

    auto* ret = encoded_next_.Decode(slot_size, std::forward<Args>(args)...);
    if (!IsWellFormed<for_thread_cache>(this, ret)) [[unlikely]] {
      // Put the corrupted data on the stack, it may give us more information
      // about what kind of corruption that was.
      PA_DEBUG_DATA_ON_STACK("first",
                             static_cast<size_t>(encoded_next_.encoded_));
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
      PA_DEBUG_DATA_ON_STACK("second", static_cast<size_t>(shadow_));
#endif
      FreelistCorruptionDetected(slot_size);
    }

    // In real-world profiles, the load of |encoded_next_| above is responsible
    // for a large fraction of the allocation cost. However, we cannot
    // anticipate it enough since it is accessed right after we know its
    // address.
    //
    // In the case of repeated allocations, we can prefetch the access that will
    // be done at the *next* allocation, which will touch *ret, prefetch it.
    PA_PREFETCH(ret);
    return ret;
  }

  template <bool for_thread_cache>
  PA_ALWAYS_INLINE static bool IsWellFormed(const FreelistEntry* here,
                                            const FreelistEntry* next) {
    // Don't allow the freelist to be blindly followed to any location.
    // Checks following constraints:
    // - `here->shadow_` must match an inversion of `here->next_` (if present).
    // - `next` mustn't point inside the super page metadata area.
    // - Unless this is a thread-cache freelist, `here` and `next` must belong
    //   to the same super page (as a matter of fact, they must belong to the
    //   same slot span, but that'd be too expensive to check here).
    // - `next` is marked as free in the free slot bitmap (if present).

    const uintptr_t here_address = SlotStartPtr2Addr(here);
    const uintptr_t next_address = SlotStartPtr2Addr(next);

#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
    bool shadow_ptr_ok = here->encoded_next_.Inverted() == here->shadow_;
#else
    constexpr bool shadow_ptr_ok = true;
#endif

    // This is necessary but not sufficient when quarantine is enabled, see
    // SuperPagePayloadBegin() in partition_page.h. However we don't want to
    // fetch anything from the root in this function.
    const bool not_in_metadata =
        (next_address & kSuperPageOffsetMask) >= PartitionPageSize();

    if constexpr (for_thread_cache) {
      return shadow_ptr_ok & not_in_metadata;
    }

    const bool same_super_page = (here_address & kSuperPageBaseMask) ==
                                 (next_address & kSuperPageBaseMask);

    return shadow_ptr_ok & same_super_page & not_in_metadata;
  }

  EncodedPtr encoded_next_;
  // This is intended to detect unintentional corruptions of the freelist.
  // These can happen due to a Use-after-Free, or overflow of the previous
  // allocation in the slot span.
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
  uintptr_t shadow_;
#endif
};

// Assertions that are agnostic to the implementation of the freelist.
static_assert(BucketIndexLookup::kMinBucketSize >=
                  sizeof(partition_alloc::internal::FreelistEntry),
              "Need enough space for freelist entries in the smallest slot");

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PARTITION_FREELIST_ENTRY_H_
