// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_

#include <stdint.h>

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/compiler_specific.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"

namespace base {
namespace internal {

struct EncodedPartitionFreelistEntry;

class PartitionFreelistEntry {
 public:
  PartitionFreelistEntry() { SetNext(nullptr); }
  ~PartitionFreelistEntry() = delete;

  // Creates a new entry, with |next| following it.
  static ALWAYS_INLINE PartitionFreelistEntry* InitForThreadCache(
      void* ptr,
      PartitionFreelistEntry* next) {
    auto* entry = reinterpret_cast<PartitionFreelistEntry*>(ptr);
    entry->SetNextForThreadCache(next);
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

  // Regular freelists always point to an entry within the same super page.
  ALWAYS_INLINE void SetNext(PartitionFreelistEntry* ptr) {
    PA_DCHECK(!ptr ||
              (reinterpret_cast<uintptr_t>(this) & kSuperPageBaseMask) ==
                  (reinterpret_cast<uintptr_t>(ptr) & kSuperPageBaseMask));
    next_ = Encode(ptr);
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

  // ThreadCache freelists can point to entries across superpage boundaries.
  ALWAYS_INLINE void SetNextForThreadCache(PartitionFreelistEntry* ptr) {
    next_ = Encode(ptr);
  }

  EncodedPartitionFreelistEntry* next_;
};

struct EncodedPartitionFreelistEntry {
  char scrambled[sizeof(PartitionFreelistEntry*)];

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
  return EncodedPartitionFreelistEntry::Decode(next_);
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_
