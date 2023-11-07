// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_ENCODED_NEXT_FREELIST_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_ENCODED_NEXT_FREELIST_H_

#include <cstddef>
#include <cstdint>

#include "build/build_config.h"
#include "partition_alloc/freeslot_bitmap.h"
#include "partition_alloc/partition_alloc-inl.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/debug/debugging_buildflags.h"
#include "partition_alloc/partition_alloc_base/immediate_crash.h"
#include "partition_alloc/partition_alloc_buildflags.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_ref_count.h"

#if !defined(ARCH_CPU_BIG_ENDIAN)
#include "partition_alloc/reverse_bytes.h"
#endif  // !defined(ARCH_CPU_BIG_ENDIAN)

namespace partition_alloc::internal {

class EncodedNextFreelistEntry;

class EncodedFreelistPtr {
 private:
  PA_ALWAYS_INLINE constexpr explicit EncodedFreelistPtr(std::nullptr_t)
      : encoded_(Transform(0)) {}
  PA_ALWAYS_INLINE explicit EncodedFreelistPtr(void* ptr)
      // The encoded pointer stays MTE-tagged.
      : encoded_(Transform(reinterpret_cast<uintptr_t>(ptr))) {}

  PA_ALWAYS_INLINE EncodedNextFreelistEntry* Decode() const {
    return reinterpret_cast<EncodedNextFreelistEntry*>(Transform(encoded_));
  }

  PA_ALWAYS_INLINE constexpr uintptr_t Inverted() const { return ~encoded_; }

  PA_ALWAYS_INLINE constexpr void Override(uintptr_t encoded) {
    encoded_ = encoded;
  }

  PA_ALWAYS_INLINE constexpr explicit operator bool() const { return encoded_; }

  // Transform() works the same in both directions, so can be used for
  // encoding and decoding.
  PA_ALWAYS_INLINE static constexpr uintptr_t Transform(uintptr_t address) {
    // We use bswap on little endian as a fast transformation for two reasons:
    // 1) On 64 bit architectures, the pointer is very unlikely to be a
    //    canonical address. Therefore, if an object is freed and its vtable is
    //    used where the attacker doesn't get the chance to run allocations
    //    between the free and use, the vtable dereference is likely to fault.
    // 2) If the attacker has a linear buffer overflow and elects to try and
    //    corrupt a freelist pointer, partial pointer overwrite attacks are
    //    thwarted.
    // For big endian, similar guarantees are arrived at with a negation.
#if defined(ARCH_CPU_BIG_ENDIAN)
    uintptr_t transformed = ~address;
#else
    uintptr_t transformed = ReverseBytes(address);
#endif
    return transformed;
  }

  uintptr_t encoded_;

  friend EncodedNextFreelistEntry;
};

// Freelist entries are encoded for security reasons. See
// //base/allocator/partition_allocator/PartitionAlloc.md
// and |Transform()| for the rationale and mechanism, respectively.
class EncodedNextFreelistEntry {
 private:
  constexpr explicit EncodedNextFreelistEntry(std::nullptr_t)
      : encoded_next_(EncodedFreelistPtr(nullptr))
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
        ,
        shadow_(encoded_next_.Inverted())
#endif
  {
  }
  explicit EncodedNextFreelistEntry(EncodedNextFreelistEntry* next)
      : encoded_next_(EncodedFreelistPtr(next))
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
        ,
        shadow_(encoded_next_.Inverted())
#endif
  {
  }
  // For testing only.
  EncodedNextFreelistEntry(void* next, bool make_shadow_match)
      : encoded_next_(EncodedFreelistPtr(next))
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
        ,
        shadow_(make_shadow_match ? encoded_next_.Inverted() : 12345)
#endif
  {
  }

 public:
  ~EncodedNextFreelistEntry() = delete;

  // Emplaces the freelist entry at the beginning of the given slot span, and
  // initializes it as null-terminated.
  PA_ALWAYS_INLINE static EncodedNextFreelistEntry* EmplaceAndInitNull(
      void* slot_start_tagged) {
    // |slot_start_tagged| is MTE-tagged.
    auto* entry = new (slot_start_tagged) EncodedNextFreelistEntry(nullptr);
    return entry;
  }
  PA_ALWAYS_INLINE static EncodedNextFreelistEntry* EmplaceAndInitNull(
      uintptr_t slot_start) {
    return EmplaceAndInitNull(SlotStartAddr2Ptr(slot_start));
  }

  // Emplaces the freelist entry at the beginning of the given slot span, and
  // initializes it with the given |next| pointer, but encoded.
  //
  // This freelist is built for the purpose of thread-cache. This means that we
  // can't perform a check that this and the next pointer belong to the same
  // super page, as thread-cache spans may chain slots across super pages.
  PA_ALWAYS_INLINE static EncodedNextFreelistEntry*
  EmplaceAndInitForThreadCache(uintptr_t slot_start,
                               EncodedNextFreelistEntry* next) {
    auto* entry =
        new (SlotStartAddr2Ptr(slot_start)) EncodedNextFreelistEntry(next);
    return entry;
  }

  // Emplaces the freelist entry at the beginning of the given slot span, and
  // initializes it with the given |next| pointer.
  //
  // This is for testing purposes only! |make_shadow_match| allows you to choose
  // if the shadow matches the next pointer properly or is trash.
  PA_ALWAYS_INLINE static void EmplaceAndInitForTest(uintptr_t slot_start,
                                                     void* next,
                                                     bool make_shadow_match) {
    new (SlotStartAddr2Ptr(slot_start))
        EncodedNextFreelistEntry(next, make_shadow_match);
  }

  void CorruptNextForTesting(uintptr_t v) {
    // We just need a value that can never be a valid pointer here.
    encoded_next_.Override(EncodedFreelistPtr::Transform(v));
  }

  // Puts `slot_size` on the stack before crashing in case of memory
  // corruption. Meant to be used to report the failed allocation size.
  template <bool crash_on_corruption>
  PA_ALWAYS_INLINE EncodedNextFreelistEntry* GetNextForThreadCache(
      size_t slot_size) const;
  PA_ALWAYS_INLINE EncodedNextFreelistEntry* GetNext(size_t slot_size) const;

  PA_NOINLINE void CheckFreeList(size_t slot_size) const {
    for (auto* entry = this; entry; entry = entry->GetNext(slot_size)) {
      // |GetNext()| checks freelist integrity.
    }
  }

  PA_NOINLINE void CheckFreeListForThreadCache(size_t slot_size) const {
    for (auto* entry = this; entry;
         entry = entry->GetNextForThreadCache<true>(slot_size)) {
      // |GetNextForThreadCache()| checks freelist integrity.
    }
  }

  PA_ALWAYS_INLINE void SetNext(EncodedNextFreelistEntry* entry) {
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

    encoded_next_ = EncodedFreelistPtr(entry);
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
  template <bool crash_on_corruption>
  PA_ALWAYS_INLINE EncodedNextFreelistEntry* GetNextInternal(
      size_t slot_size,
      bool for_thread_cache) const;

  PA_ALWAYS_INLINE static bool IsSane(const EncodedNextFreelistEntry* here,
                                      const EncodedNextFreelistEntry* next,
                                      bool for_thread_cache) {
    // Don't allow the freelist to be blindly followed to any location.
    // Checks two constraints:
    // - here and next must belong to the same superpage, unless this is in the
    //   thread cache (they even always belong to the same slot span).
    // - next cannot point inside the metadata area.
    //
    // Also, the lightweight UaF detection (pointer shadow) is checked.

    uintptr_t here_address = SlotStartPtr2Addr(here);
    uintptr_t next_address = SlotStartPtr2Addr(next);

#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
    bool shadow_ptr_ok = here->encoded_next_.Inverted() == here->shadow_;
#else
    bool shadow_ptr_ok = true;
#endif

    bool same_superpage = (here_address & kSuperPageBaseMask) ==
                          (next_address & kSuperPageBaseMask);
#if BUILDFLAG(USE_FREESLOT_BITMAP)
    bool marked_as_free_in_bitmap =
        for_thread_cache ? true : !FreeSlotBitmapSlotIsUsed(next_address);
#else
    bool marked_as_free_in_bitmap = true;
#endif

    // This is necessary but not sufficient when quarantine is enabled, see
    // SuperPagePayloadBegin() in partition_page.h. However we don't want to
    // fetch anything from the root in this function.
    bool not_in_metadata =
        (next_address & kSuperPageOffsetMask) >= PartitionPageSize();

    if (for_thread_cache) {
      return shadow_ptr_ok & not_in_metadata;
    } else {
      return shadow_ptr_ok & same_superpage & marked_as_free_in_bitmap &
             not_in_metadata;
    }
  }

  EncodedFreelistPtr encoded_next_;
  // This is intended to detect unintentional corruptions of the freelist.
  // These can happen due to a Use-after-Free, or overflow of the previous
  // allocation in the slot span.
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
  uintptr_t shadow_;
#endif
};

template <bool crash_on_corruption>
PA_ALWAYS_INLINE EncodedNextFreelistEntry*
EncodedNextFreelistEntry::GetNextInternal(size_t slot_size,
                                          bool for_thread_cache) const {
  // GetNext() can be called on discarded memory, in which case |encoded_next_|
  // is 0, and none of the checks apply. Don't prefetch nullptr either.
  if (IsEncodedNextPtrZero()) {
    return nullptr;
  }

  auto* ret = encoded_next_.Decode();
  // We rely on constant propagation to remove the branches coming from
  // |for_thread_cache|, since the argument is always a compile-time constant.
  if (PA_UNLIKELY(!IsSane(this, ret, for_thread_cache))) {
    if constexpr (crash_on_corruption) {
      // Put the corrupted data on the stack, it may give us more information
      // about what kind of corruption that was.
      PA_DEBUG_DATA_ON_STACK("first",
                             static_cast<size_t>(encoded_next_.encoded_));
#if PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)
      PA_DEBUG_DATA_ON_STACK("second", static_cast<size_t>(shadow_));
#endif
      FreelistCorruptionDetected(slot_size);
    } else {
      return nullptr;
    }
  }

  // In real-world profiles, the load of |encoded_next_| above is responsible
  // for a large fraction of the allocation cost. However, we cannot anticipate
  // it enough since it is accessed right after we know its address.
  //
  // In the case of repeated allocations, we can prefetch the access that will
  // be done at the *next* allocation, which will touch *ret, prefetch it.
  PA_PREFETCH(ret);

  return ret;
}

template <bool crash_on_corruption>
PA_ALWAYS_INLINE EncodedNextFreelistEntry*
EncodedNextFreelistEntry::GetNextForThreadCache(size_t slot_size) const {
  return GetNextInternal<crash_on_corruption>(slot_size, true);
}

PA_ALWAYS_INLINE EncodedNextFreelistEntry* EncodedNextFreelistEntry::GetNext(
    size_t slot_size) const {
  return GetNextInternal<true>(slot_size, false);
}

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_ENCODED_NEXT_FREELIST_H_
