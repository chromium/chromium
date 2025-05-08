// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_ENCODED_NEXT_FREELIST_H_
#define PARTITION_ALLOC_ENCODED_NEXT_FREELIST_H_

#include <cstddef>
#include <cstdint>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc-inl.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"

#if !PA_BUILDFLAG(PA_ARCH_CPU_BIG_ENDIAN)
#include "partition_alloc/reverse_bytes.h"
#endif

namespace partition_alloc::internal {

// Defined in "partition_freelist_entry.cc".
[[noreturn]] PA_NOINLINE PA_COMPONENT_EXPORT(
    PARTITION_ALLOC) void FreelistCorruptionDetected(size_t slot_size);

class FreelistEntry;

class EncodedFreelistPtr {
 private:
  PA_ALWAYS_INLINE constexpr explicit EncodedFreelistPtr(std::nullptr_t)
      : encoded_(Transform(0)) {}
  PA_ALWAYS_INLINE explicit EncodedFreelistPtr(void* ptr)
      // The encoded pointer stays MTE-tagged.
      : encoded_(Transform(reinterpret_cast<uintptr_t>(ptr))) {}

  PA_ALWAYS_INLINE FreelistEntry* Decode(size_t unused_slot_size) const {
    return reinterpret_cast<FreelistEntry*>(Transform(encoded_));
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
    // 1) On 64 bit architectures, the swapped pointer is very unlikely to be a
    //    canonical address. Therefore, if an object is freed and its vtable is
    //    used where the attacker doesn't get the chance to run allocations
    //    between the free and use, the vtable dereference is likely to fault.
    // 2) If the attacker has a linear buffer overflow and elects to try and
    //    corrupt a freelist pointer, partial pointer overwrite attacks are
    //    thwarted.
    // For big endian, similar guarantees are arrived at with a negation.
#if PA_BUILDFLAG(PA_ARCH_CPU_BIG_ENDIAN)
    uintptr_t transformed = ~address;
#else
    uintptr_t transformed = ReverseBytes(address);
#endif
    return transformed;
  }

  uintptr_t encoded_;

  friend FreelistEntry;
};

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_ENCODED_NEXT_FREELIST_H_
