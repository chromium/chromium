// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_

#include <stdint.h>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc-inl.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/debug/alias.h"
#include "base/immediate_crash.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"

namespace base {
namespace internal {

namespace {

#if defined(PA_HAS_FREELIST_HARDENING) || DCHECK_IS_ON()
[[noreturn]] NOINLINE void FreelistCorruptionDetected(size_t extra) {
  // Make it visible in minidumps.
  //
  // To make the size stick out, surround it with two easily recognizable
  // patterns: 0xffffffff..
  // Locally, one can use "x/3g <%rsp address>" in GDB to see the value on
  // x86_64.
  size_t before = ~0;
  base::debug::Alias(&before);

  size_t tmp_extra = extra;
  base::debug::Alias(&tmp_extra);

  size_t after = ~0;
  base::debug::Alias(&after);

  IMMEDIATE_CRASH();
}
#endif  // defined(PA_HAS_FREELIST_HARDENING) || DCHECK_IS_ON()

}  // namespace

struct EncodedPartitionFreelistEntry;

#if defined(PA_HAS_FREELIST_HARDENING)
static_assert(kSmallestBucket >= 2 * sizeof(void*),
              "Need enough space for two pointers in freelist entries");
#endif

#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
constexpr size_t kMinimalBucketSizeWithRefCount =
    (1 + sizeof(PartitionRefCount) + kSmallestBucket - 1) &
    ~(kSmallestBucket - 1);
#if defined(PA_HAS_FREELIST_HARDENING)
static_assert(
    kMinimalBucketSizeWithRefCount >=
        sizeof(PartitionRefCount) + 2 * sizeof(void*),
    "Need enough space for two pointer and one refcount in freelist entries");
#else
static_assert(
    kMinimalBucketSizeWithRefCount >= sizeof(PartitionRefCount) + sizeof(void*),
    "Need enough space for one pointer and one refcount in freelist entries");
#endif
#endif

// Freelist entries are encoded for security reasons. See
// //base/allocator/partition_allocator/PartitionAlloc.md and |Transform()| for
// the rationale and mechanism, respectively.
class PartitionFreelistEntry {
 public:
  PartitionFreelistEntry() { SetNext(nullptr); }
  ~PartitionFreelistEntry() = delete;

  // Creates a new entry, with |next| following it.
  static ALWAYS_INLINE PartitionFreelistEntry* InitForThreadCache(
      void* slot_start,
      PartitionFreelistEntry* next) {
    auto* entry = reinterpret_cast<PartitionFreelistEntry*>(slot_start);
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

  // Puts |extra| on the stack before crashing in case of memory
  // corruption. Meant to be used to report the failed allocation size.
  ALWAYS_INLINE PartitionFreelistEntry* GetNext(size_t extra) const;
  NOINLINE void CheckFreeList(size_t extra) const {
#if defined(PA_HAS_FREELIST_HARDENING)
    for (auto* entry = this; entry; entry = entry->GetNext(extra)) {
      // |GetNext()| checks freelist integrity.
    }
#endif
  }

  ALWAYS_INLINE void SetNext(PartitionFreelistEntry* ptr) {
    // SetNext() is either called on the freelist head, when provisioning new
    // slots, or when GetNext() has been called before, no need to pass the
    // size.
#if DCHECK_IS_ON()
    // Regular freelists always point to an entry within the same super page.
    //
    // This is most likely a PartitionAlloc bug if this triggers.
    if (UNLIKELY(ptr &&
                 (reinterpret_cast<uintptr_t>(this) & kSuperPageBaseMask) !=
                     (reinterpret_cast<uintptr_t>(ptr) & kSuperPageBaseMask))) {
      FreelistCorruptionDetected(0);
    }
#endif  // DCHECK_IS_ON()
    SetNextInternal(ptr);
  }

  // Zeroes out |this| before returning it.
  ALWAYS_INLINE void* ClearForAllocation() {
    next_ = nullptr;
#if defined(PA_HAS_FREELIST_HARDENING)
    inverted_next_ = 0;
#endif
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
#if defined(PA_HAS_FREELIST_HARDENING)
    inverted_next_ = ~reinterpret_cast<uintptr_t>(next_);
#endif
  }

  EncodedPartitionFreelistEntry* next_;
  // This is intended to detect unintentional corruptions of the freelist.
  // These can happen due to a Use-after-Free, or overflow of the previous
  // allocation in the slot span.
#if defined(PA_HAS_FREELIST_HARDENING)
  uintptr_t inverted_next_;
#endif
};

struct EncodedPartitionFreelistEntry {
  char scrambled[sizeof(PartitionFreelistEntry*)];
#if defined(PA_HAS_FREELIST_HARDENING)
  char copy_of_scrambled[sizeof(PartitionFreelistEntry*)];
#endif

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

ALWAYS_INLINE PartitionFreelistEntry* PartitionFreelistEntry::GetNext(
    size_t extra) const {
#if defined(PA_HAS_FREELIST_HARDENING)
  // GetNext() can be called on decommitted memory, which is full of
  // zeroes. This is not a corruption issue, so only check integrity when we
  // have a non-nullptr |next_| pointer.
  if (UNLIKELY(next_ && ~reinterpret_cast<uintptr_t>(next_) != inverted_next_))
    FreelistCorruptionDetected(extra);
#endif  // defined(PA_HAS_FREELIST_HARDENING)
  auto* ret = EncodedPartitionFreelistEntry::Decode(next_);
  // In real-world profiles, the load of |next_| above is responsible for a
  // large fraction of the allocation cost. However, we cannot anticipate it
  // enough since it is accessed right after we know its address.
  //
  // In the case of repeated allocations, we can prefetch the access that will
  // be done at the *next* allocation, which will touch *ret, prefetch it. There
  // is no harm in prefetching nullptr, but on some architectures, it causes a
  // needless dTLB miss.
  if (ret)
    PA_PREFETCH(ret);
  return ret;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_
