// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_FREELIST_ENTRY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_FREELIST_ENTRY_H_

#include <cstddef>

#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_buildflags.h"
#include "partition_alloc/partition_alloc_constants.h"

namespace partition_alloc::internal {

[[noreturn]] PA_NOINLINE PA_COMPONENT_EXPORT(
    PARTITION_ALLOC) void FreelistCorruptionDetected(size_t slot_size);

}  // namespace partition_alloc::internal

#include "partition_alloc/encoded_next_freelist.h"  // IWYU pragma: export

// PA defaults to a freelist whose "next" links are encoded pointers.
// We are assessing an alternate implementation using an alternate
// encoding (pool offsets). When build support is enabled, the
// freelist implementation is determined at runtime.
#if BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
#include "partition_alloc/pool_offset_freelist.h"  // IWYU pragma: export
#endif  // BUILDFLAG(USE_FREELIST_POOL_OFFSETS)

namespace partition_alloc::internal {

// Assertions that are agnostic to the implementation of the freelist.

static_assert(kSmallestBucket >= sizeof(EncodedNextFreelistEntry),
              "Need enough space for freelist entries in the smallest slot");
#if BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
static_assert(kSmallestBucket >= sizeof(PoolOffsetFreelistEntry),
              "Need enough space for freelist entries in the smallest slot");
#endif  // BUILDFLAG(USE_FREELIST_POOL_OFFSETS)

enum class PartitionFreelistEncoding {
  kEncodedFreeList,
  kPoolOffsetFreeList,
};

#if BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
union PartitionFreelistEntry {
  EncodedNextFreelistEntry encoded_entry_;
  PoolOffsetFreelistEntry pool_offset_entry_;
};
#else
using PartitionFreelistEntry = EncodedNextFreelistEntry;
#endif  // USE_FREELIST_POOL_OFFSETS

#if BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
static_assert(offsetof(PartitionFreelistEntry, encoded_entry_) == 0ull);
static_assert(offsetof(PartitionFreelistEntry, pool_offset_entry_) == 0ull);
#endif  // BUILDFLAG(USE_FREELIST_POOL_OFFSETS)

struct PartitionFreelistDispatcher {
#if BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
  static const PartitionFreelistDispatcher* Create(
      PartitionFreelistEncoding encoding);

  PA_ALWAYS_INLINE virtual PartitionFreelistEntry* EmplaceAndInitNull(
      void* slot_start_tagged) const = 0;
  PA_ALWAYS_INLINE virtual PartitionFreelistEntry* EmplaceAndInitNull(
      uintptr_t slot_start) const = 0;
  PA_ALWAYS_INLINE virtual PartitionFreelistEntry* EmplaceAndInitForThreadCache(
      uintptr_t slot_start,
      PartitionFreelistEntry* next) const = 0;
  PA_ALWAYS_INLINE virtual void EmplaceAndInitForTest(
      uintptr_t slot_start,
      void* next,
      bool make_shadow_match) const = 0;
  PA_ALWAYS_INLINE virtual void CorruptNextForTesting(
      PartitionFreelistEntry* entry,
      uintptr_t v) const = 0;
  PA_ALWAYS_INLINE virtual PartitionFreelistEntry* GetNextForThreadCacheTrue(
      PartitionFreelistEntry* entry,
      size_t slot_size) const = 0;
  PA_ALWAYS_INLINE virtual PartitionFreelistEntry* GetNextForThreadCacheFalse(
      PartitionFreelistEntry* entry,
      size_t slot_size) const = 0;
  PA_ALWAYS_INLINE virtual PartitionFreelistEntry* GetNextForThreadCacheBool(
      PartitionFreelistEntry* entry,
      bool crash_on_corruption,
      size_t slot_size) const = 0;
  PA_ALWAYS_INLINE virtual PartitionFreelistEntry* GetNext(
      PartitionFreelistEntry* entry,
      size_t slot_size) const = 0;
  PA_NOINLINE virtual void CheckFreeList(PartitionFreelistEntry* entry,
                                         size_t slot_size) const = 0;
  PA_NOINLINE virtual void CheckFreeListForThreadCache(
      PartitionFreelistEntry* entry,
      size_t slot_size) const = 0;
  PA_ALWAYS_INLINE virtual void SetNext(PartitionFreelistEntry* entry,
                                        PartitionFreelistEntry* next) const = 0;
  PA_ALWAYS_INLINE virtual uintptr_t ClearForAllocation(
      PartitionFreelistEntry* entry) const = 0;
  PA_ALWAYS_INLINE virtual constexpr bool IsEncodedNextPtrZero(
      PartitionFreelistEntry* entry) const = 0;

  virtual ~PartitionFreelistDispatcher() = default;
#else
  static const PartitionFreelistDispatcher* Create(
      PartitionFreelistEncoding encoding) {
    static constinit PartitionFreelistDispatcher dispatcher =
        PartitionFreelistDispatcher();
    return &dispatcher;
  }

  PA_ALWAYS_INLINE PartitionFreelistEntry* EmplaceAndInitNull(
      void* slot_start_tagged) const {
    return reinterpret_cast<PartitionFreelistEntry*>(
        EncodedNextFreelistEntry::EmplaceAndInitNull(slot_start_tagged));
  }

  PA_ALWAYS_INLINE PartitionFreelistEntry* EmplaceAndInitNull(
      uintptr_t slot_start) const {
    return reinterpret_cast<PartitionFreelistEntry*>(
        EncodedNextFreelistEntry::EmplaceAndInitNull(slot_start));
  }

  PA_ALWAYS_INLINE PartitionFreelistEntry* EmplaceAndInitForThreadCache(
      uintptr_t slot_start,
      PartitionFreelistEntry* next) const {
    return reinterpret_cast<PartitionFreelistEntry*>(
        EncodedNextFreelistEntry::EmplaceAndInitForThreadCache(slot_start,
                                                               next));
  }

  PA_ALWAYS_INLINE void EmplaceAndInitForTest(uintptr_t slot_start,
                                              void* next,
                                              bool make_shadow_match) const {
    return EncodedNextFreelistEntry::EmplaceAndInitForTest(slot_start, next,
                                                           make_shadow_match);
  }

  PA_ALWAYS_INLINE void CorruptNextForTesting(PartitionFreelistEntry* entry,
                                              uintptr_t v) const {
    return entry->CorruptNextForTesting(v);
  }

  template <bool crash_on_corruption>
  PA_ALWAYS_INLINE PartitionFreelistEntry* GetNextForThreadCache(
      PartitionFreelistEntry* entry,
      size_t slot_size) const {
    return reinterpret_cast<PartitionFreelistEntry*>(
        entry->GetNextForThreadCache<crash_on_corruption>(slot_size));
  }

  PA_ALWAYS_INLINE PartitionFreelistEntry* GetNext(
      PartitionFreelistEntry* entry,
      size_t slot_size) const {
    return reinterpret_cast<PartitionFreelistEntry*>(entry->GetNext(slot_size));
  }

  PA_NOINLINE void CheckFreeList(PartitionFreelistEntry* entry,
                                 size_t slot_size) const {
    return entry->CheckFreeList(slot_size);
  }

  PA_NOINLINE void CheckFreeListForThreadCache(PartitionFreelistEntry* entry,
                                               size_t slot_size) const {
    return entry->CheckFreeListForThreadCache(slot_size);
  }

  PA_ALWAYS_INLINE void SetNext(PartitionFreelistEntry* entry,
                                PartitionFreelistEntry* next) const {
    return entry->SetNext(next);
  }

  PA_ALWAYS_INLINE uintptr_t
  ClearForAllocation(PartitionFreelistEntry* entry) const {
    return entry->ClearForAllocation();
  }

  PA_ALWAYS_INLINE constexpr bool IsEncodedNextPtrZero(
      PartitionFreelistEntry* entry) const {
    return entry->IsEncodedNextPtrZero();
  }

  ~PartitionFreelistDispatcher() = default;
#endif  // BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
};

#if BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
template <PartitionFreelistEncoding encoding>
struct PartitionFreelistDispatcherImpl : PartitionFreelistDispatcher {
  using Entry =
      std::conditional_t<encoding ==
                             PartitionFreelistEncoding::kEncodedFreeList,
                         EncodedNextFreelistEntry,
                         PoolOffsetFreelistEntry>;

  Entry& GetEntryImpl(PartitionFreelistEntry* entry) const {
    if constexpr (encoding == PartitionFreelistEncoding::kEncodedFreeList) {
      return entry->encoded_entry_;
    } else {
      return entry->pool_offset_entry_;
    }
  }

  PA_ALWAYS_INLINE PartitionFreelistEntry* EmplaceAndInitNull(
      void* slot_start_tagged) const override {
    return reinterpret_cast<PartitionFreelistEntry*>(
        Entry::EmplaceAndInitNull(slot_start_tagged));
  }

  PA_ALWAYS_INLINE PartitionFreelistEntry* EmplaceAndInitNull(
      uintptr_t slot_start) const override {
    return reinterpret_cast<PartitionFreelistEntry*>(
        Entry::EmplaceAndInitNull(slot_start));
  }

  PA_ALWAYS_INLINE PartitionFreelistEntry* EmplaceAndInitForThreadCache(
      uintptr_t slot_start,
      PartitionFreelistEntry* next) const override {
    if constexpr (encoding == PartitionFreelistEncoding::kEncodedFreeList) {
      return reinterpret_cast<PartitionFreelistEntry*>(
          Entry::EmplaceAndInitForThreadCache(slot_start,
                                              &(next->encoded_entry_)));
    } else {
      return reinterpret_cast<PartitionFreelistEntry*>(
          Entry::EmplaceAndInitForThreadCache(slot_start,
                                              &(next->pool_offset_entry_)));
    }
  }

  PA_ALWAYS_INLINE void EmplaceAndInitForTest(
      uintptr_t slot_start,
      void* next,
      bool make_shadow_match) const override {
    return Entry::EmplaceAndInitForTest(slot_start, next, make_shadow_match);
  }

  PA_ALWAYS_INLINE void CorruptNextForTesting(PartitionFreelistEntry* entry,
                                              uintptr_t v) const override {
    return GetEntryImpl(entry).CorruptNextForTesting(v);
  }

  PA_ALWAYS_INLINE PartitionFreelistEntry* GetNextForThreadCacheTrue(
      PartitionFreelistEntry* entry,
      size_t slot_size) const override {
    return reinterpret_cast<PartitionFreelistEntry*>(
        GetEntryImpl(entry).template GetNextForThreadCache<true>(slot_size));
  }

  PA_ALWAYS_INLINE PartitionFreelistEntry* GetNextForThreadCacheFalse(
      PartitionFreelistEntry* entry,
      size_t slot_size) const override {
    return reinterpret_cast<PartitionFreelistEntry*>(
        GetEntryImpl(entry).template GetNextForThreadCache<false>(slot_size));
  }

  PA_ALWAYS_INLINE PartitionFreelistEntry* GetNextForThreadCacheBool(
      PartitionFreelistEntry* entry,
      bool crash_on_corruption,
      size_t slot_size) const override {
    if (crash_on_corruption) {
      return GetNextForThreadCacheTrue(entry, slot_size);
    } else {
      return GetNextForThreadCacheFalse(entry, slot_size);
    }
  }

  PA_ALWAYS_INLINE PartitionFreelistEntry* GetNext(
      PartitionFreelistEntry* entry,
      size_t slot_size) const override {
    return reinterpret_cast<PartitionFreelistEntry*>(
        GetEntryImpl(entry).GetNext(slot_size));
  }

  PA_NOINLINE void CheckFreeList(PartitionFreelistEntry* entry,
                                 size_t slot_size) const override {
    return GetEntryImpl(entry).CheckFreeList(slot_size);
  }

  PA_NOINLINE void CheckFreeListForThreadCache(
      PartitionFreelistEntry* entry,
      size_t slot_size) const override {
    return GetEntryImpl(entry).CheckFreeListForThreadCache(slot_size);
  }

  PA_ALWAYS_INLINE void SetNext(PartitionFreelistEntry* entry,
                                PartitionFreelistEntry* next) const override {
    if constexpr (encoding == PartitionFreelistEncoding::kEncodedFreeList) {
      return GetEntryImpl(entry).SetNext(&(next->encoded_entry_));
    } else {
      return GetEntryImpl(entry).SetNext(&(next->pool_offset_entry_));
    }
  }

  PA_ALWAYS_INLINE uintptr_t
  ClearForAllocation(PartitionFreelistEntry* entry) const override {
    return GetEntryImpl(entry).ClearForAllocation();
  }

  PA_ALWAYS_INLINE constexpr bool IsEncodedNextPtrZero(
      PartitionFreelistEntry* entry) const override {
    return GetEntryImpl(entry).IsEncodedNextPtrZero();
  }
};

PA_ALWAYS_INLINE const PartitionFreelistDispatcher*
PartitionFreelistDispatcher::Create(PartitionFreelistEncoding encoding) {
  switch (encoding) {
    case PartitionFreelistEncoding::kEncodedFreeList: {
      static constinit PartitionFreelistDispatcherImpl<
          PartitionFreelistEncoding::kEncodedFreeList>
          encoded = PartitionFreelistDispatcherImpl<
              PartitionFreelistEncoding::kEncodedFreeList>();
      return &encoded;
    }
    case PartitionFreelistEncoding::kPoolOffsetFreeList: {
      static constinit PartitionFreelistDispatcherImpl<
          PartitionFreelistEncoding::kPoolOffsetFreeList>
          pool = PartitionFreelistDispatcherImpl<
              PartitionFreelistEncoding::kPoolOffsetFreeList>();
      return &pool;
    }
  }
}
#endif  // USE_FREELIST_POOL_OFFSETS
}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_FREELIST_ENTRY_H_
