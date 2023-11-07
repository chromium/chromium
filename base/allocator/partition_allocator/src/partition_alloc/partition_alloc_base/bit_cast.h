// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_BIT_CAST_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_BIT_CAST_H_

#include <type_traits>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/compiler_specific.h"

#if !PA_HAS_BUILTIN(__builtin_bit_cast)
#include <string.h>  // memcpy
#endif

namespace partition_alloc::internal::base {

// This is C++20's std::bit_cast<>().
// It morally does what `*reinterpret_cast<Dest*>(&source)` does, but the
// cast/deref pair is undefined behavior, while bit_cast<>() isn't.
template <class Dest, class Source>
#if PA_HAS_BUILTIN(__builtin_bit_cast)
constexpr
#else
inline
#endif
    Dest
    bit_cast(const Source& source) {
#if PA_HAS_BUILTIN(__builtin_bit_cast)
  // TODO(thakis): Keep only this codepath once nacl is gone or updated.
  return __builtin_bit_cast(Dest, source);
#else
  static_assert(sizeof(Dest) == sizeof(Source),
                "bit_cast requires source and destination to be the same size");
  static_assert(std::is_trivially_copyable_v<Dest>,
                "bit_cast requires the destination type to be copyable");
  static_assert(std::is_trivially_copyable_v<Source>,
                "bit_cast requires the source type to be copyable");

  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
#endif
}

}  // namespace partition_alloc::internal::base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_BIT_CAST_H_
