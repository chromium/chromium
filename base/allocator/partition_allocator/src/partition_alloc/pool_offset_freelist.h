// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef PARTITION_ALLOC_POOL_OFFSET_FREELIST_H_
#define PARTITION_ALLOC_POOL_OFFSET_FREELIST_H_

#include <cstddef>
#include <cstdint>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc-inl.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/tagging.h"

#if !PA_BUILDFLAG(PA_ARCH_CPU_BIG_ENDIAN)
#include "partition_alloc/reverse_bytes.h"
#endif

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#error Pool-Offset Freelist is supported only on 64-bit system.
#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)

namespace partition_alloc::internal {

// Defined in "partition_freelist_entry.cc".
[[noreturn]] PA_NOINLINE PA_COMPONENT_EXPORT(
    PARTITION_ALLOC) void FreelistCorruptionDetected(size_t slot_size);

using PoolInfo = PartitionAddressSpace::PoolInfo;

class FreelistEntry;

// Encoding to store the entries as pool offsets. In a scenario that an attacker
// has a write primitive anywhere within the pool, they would not be able to
// corrupt the freelist in a way that would allow them to break out of the pool.
class EncodedPoolOffset {
#if PA_BUILDFLAG(PA_ARCH_CPU_BIG_ENDIAN)
  static constexpr uintptr_t kEncodeedNullptr = ~uintptr_t{0};
#else
  static constexpr uintptr_t kEncodeedNullptr = uintptr_t{0};
#endif

  PA_ALWAYS_INLINE constexpr explicit EncodedPoolOffset(std::nullptr_t)
      : encoded_(kEncodeedNullptr) {}
  PA_ALWAYS_INLINE explicit EncodedPoolOffset(void* ptr)
      // The encoded pointer stays MTE-tagged.
      : encoded_(Encode(ptr)) {}
  // Similar to above, but faster with known pool.
  PA_ALWAYS_INLINE explicit EncodedPoolOffset(
      void* ptr,
      const PoolOffsetLookup& offset_lookup)
      : encoded_(Encode(ptr, offset_lookup)) {}

  PA_ALWAYS_INLINE constexpr uintptr_t Inverted() const { return ~encoded_; }

  PA_ALWAYS_INLINE constexpr void Override(uintptr_t encoded) {
    encoded_ = encoded;
  }

  PA_ALWAYS_INLINE constexpr explicit operator bool() const { return encoded_; }

  // Transform() works the same in both directions, so can be used for
  // encoding and decoding.
  PA_ALWAYS_INLINE static constexpr uintptr_t Transform(uintptr_t offset) {
    // We use bswap on little endian as a fast transformation for two reasons:
    // 1) The offset is a canonical address, possibly pointing to valid memory,
    //    whereas, on 64 bit, the swapped offset is very unlikely to be a
    //    canonical address. Therefore, if an object is freed and its vtable is
    //    used where the attacker doesn't get the chance to run allocations
    //    between the free and use, the vtable dereference is likely to fault.
    // 2) If the attacker has a linear buffer overflow and elects to try and
    //    corrupt a freelist pointer, partial pointer overwrite attacks are
    //    thwarted.
    // For big endian, similar guarantees are arrived at with a negation.
#if PA_BUILDFLAG(PA_ARCH_CPU_BIG_ENDIAN)
    uintptr_t transformed = ~offset;
#else
    uintptr_t transformed = ReverseBytes(offset);
#endif
    return transformed;
  }

  // Determines the containing pool of `ptr` and returns `ptr`
  // represented as a tagged offset into that pool.
  PA_ALWAYS_INLINE static uintptr_t Encode(void* ptr) {
    if (!ptr) {
      return kEncodeedNullptr;
    }
    uintptr_t address = SlotStart::Unchecked(ptr).Untag().value();
    PoolInfo pool_info = PartitionAddressSpace::GetPoolInfo(address);
    // Save a MTE tag as well as an offset.
    uintptr_t tagged_offset =
        reinterpret_cast<uintptr_t>(ptr) & (kPtrTagMask | ~pool_info.base_mask);
    return Transform(tagged_offset);
  }

  // Similar to above, but faster with known pool.
  PA_ALWAYS_INLINE static uintptr_t Encode(
      void* ptr,
      const PoolOffsetLookup& offset_lookup) {
    if (!ptr) {
      return kEncodeedNullptr;
    }
    // Save a MTE tag as well as an offset.
    return Transform(offset_lookup.GetTaggedOffset(ptr));
  }

  // Given `pool_info`, decodes a `tagged_offset` into a tagged pointer.
  PA_ALWAYS_INLINE FreelistEntry* Decode(size_t slot_size) const {
    PoolInfo pool_info =
        GetPoolInfo(SlotStart::Unchecked(this).Untag().value());
    uintptr_t tagged_offset = Transform(encoded_);

    // `tagged_offset` must not have bits set in the pool base mask, except MTE
    // tag.
    if (tagged_offset & pool_info.base_mask & ~kPtrTagMask) {
      FreelistCorruptionDetected(slot_size);
    }

    // We assume `tagged_offset` contains a proper MTE tag.
    return reinterpret_cast<FreelistEntry*>(pool_info.base | tagged_offset);
  }

  // Given `pool_info`, decodes a `tagged_offset` into a tagged pointer.
  PA_ALWAYS_INLINE FreelistEntry* Decode(
      size_t slot_size,
      const PoolOffsetLookup& offset_lookup) const {
    uintptr_t tagged_offset = Transform(encoded_);

    // `tagged_offset` must not have bits set in the pool base mask, except MTE
    // tag.
    if (!offset_lookup.IsValidTaggedOffset(tagged_offset)) {
      FreelistCorruptionDetected(slot_size);
    }

    // We assume `tagged_offset` contains a proper MTE tag.
    return static_cast<FreelistEntry*>(offset_lookup.GetPointer(tagged_offset));
  }

  uintptr_t encoded_;

  friend FreelistEntry;
};

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_POOL_OFFSET_FREELIST_H_
