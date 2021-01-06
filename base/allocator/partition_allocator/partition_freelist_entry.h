// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_

#include <stdint.h>

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/immediate_crash.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"

namespace base {
namespace internal {

namespace {

[[noreturn]] NOINLINE void FreelistCorruptionDetected() {
  IMMEDIATE_CRASH();
}

}  // namespace

struct EncodedPartitionFreelistEntry;

static_assert((1 << kMinBucketedOrder) >= 2 * sizeof(void*),
              "Need enough space for two pointers in freelist entries");

// Freelist entries are encoded for security reasons. See
// //base/allocator/partition_allocator/PartitionAlloc.md and |Transform()| for
// the rationale and mechanism, respectively.
class PartitionFreelistEntry {
 public:
  PartitionFreelistEntry() { SetNext(nullptr); }
  ~PartitionFreelistEntry() = delete;

  // Creates a new entry, with |next| following it.
  static ALWAYS_INLINE PartitionFreelistEntry* InitForThreadCache(
      void* ptr,
      PartitionFreelistEntry* next) {
    auto* entry = reinterpret_cast<PartitionFreelistEntry*>(ptr);
    // ThreadCache freelists can point to entries across superpage boundaries,
    // no check contrary to |SetNext()|.
    entry->SetNextInternal(next);
    return entry;
  }

  // Placement new only.
  void* operator new(size_t) = delete;
  void operator delete(void* ptr) = delete;
  void* operator new(size_t, void* buffer) { return buffer; }

  ALWAYS_INLINE static EncodedPartitionFreelistEntry* Encode(
      PartitionFreelistEntry* ptr) {
    return reinterpret_cast<EncodedPartitionFreelistEntry*>(Transform(ptr));
  }

  ALWAYS_INLINE PartitionFreelistEntry* GetNext() const;
  NOINLINE void CheckFreeList() const {
    for (auto* entry = this; entry; entry = entry->GetNext()) {
      // |GetNext()| checks freelist integrity.
    }
  }

  ALWAYS_INLINE void SetNext(PartitionFreelistEntry* ptr) {
#if DCHECK_IS_ON()
    // Regular freelists always point to an entry within the same super page.
    if (UNLIKELY(ptr &&
                 (reinterpret_cast<uintptr_t>(this) & kSuperPageBaseMask) !=
                     (reinterpret_cast<uintptr_t>(ptr) & kSuperPageBaseMask))) {
      FreelistCorruptionDetected();
    }
#endif
    SetNextInternal(ptr);
  }

  // Zeroes out |this| before returning it.
  ALWAYS_INLINE void* ClearForAllocation() {
    next_ = nullptr;
    inverted_next_ = 0;
    return reinterpret_cast<void*>(this);
  }

 private:
  friend struct EncodedPartitionFreelistEntry;
  ALWAYS_INLINE static void* Transform(void* ptr) {
    // We use bswap on little endian as a fast mask for two reasons:
    // 1) If an object is freed and its vtable used where the attacker doesn't
    // get the chance to run allocations between the free and use, the vtable
    // dereference is likely to fault.
    // 2) If the attacker has a linear buffer overflow and elects to try and
    // corrupt a freelist pointer, partial pointer overwrite attacks are
    // thwarted.
    // For big endian, similar guarantees are arrived at with a negation.
#if defined(ARCH_CPU_BIG_ENDIAN)
    uintptr_t masked = ~reinterpret_cast<uintptr_t>(ptr);
#else
    uintptr_t masked = ByteSwapUintPtrT(reinterpret_cast<uintptr_t>(ptr));
#endif
    return reinterpret_cast<void*>(masked);
  }

  ALWAYS_INLINE void SetNextInternal(PartitionFreelistEntry* ptr) {
    next_ = Encode(ptr);
    inverted_next_ = ~reinterpret_cast<uintptr_t>(next_);
  }

  EncodedPartitionFreelistEntry* next_;
  // This is intended to detect unintentional corruptions of the freelist.
  // These can happen due to a Use-after-Free, or overflow of the previous
  // allocation in the slot span.
  uintptr_t inverted_next_;
};

struct EncodedPartitionFreelistEntry {
  char scrambled[sizeof(PartitionFreelistEntry*)];
  char copy_of_scrambled[sizeof(PartitionFreelistEntry*)];

  EncodedPartitionFreelistEntry() = delete;
  ~EncodedPartitionFreelistEntry() = delete;

  ALWAYS_INLINE static PartitionFreelistEntry* Decode(
      EncodedPartitionFreelistEntry* ptr) {
    return reinterpret_cast<PartitionFreelistEntry*>(
        PartitionFreelistEntry::Transform(ptr));
  }
};

static_assert(sizeof(PartitionFreelistEntry) ==
                  sizeof(EncodedPartitionFreelistEntry),
              "Should not have padding");

ALWAYS_INLINE PartitionFreelistEntry* PartitionFreelistEntry::GetNext() const {
  // GetNext() can be called on decommitted memory, which is full of
  // zeroes. This is not a corruption issue, so only check integrity when we
  // have a non-nullptr |next_| pointer.
  if (UNLIKELY(next_ && ~reinterpret_cast<uintptr_t>(next_) != inverted_next_))
    FreelistCorruptionDetected();
  return EncodedPartitionFreelistEntry::Decode(next_);
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_
