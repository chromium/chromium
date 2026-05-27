// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_FREE_HINT_H_
#define PARTITION_ALLOC_FREE_HINT_H_

#include <cstddef>

#include "partition_alloc/partition_alloc_constants.h"

namespace partition_alloc {

namespace internal {

template <typename EnumType>
constexpr inline EnumType FreeHintFlags(EnumType superset) {
  return (superset &
          (FreeFlags::kWithSizeHint | FreeFlags::kWithAlignmentHint));
}

}  // namespace internal

// To avoid initializing and copying unused members, `FreeHint` is a template
// struct and tightly coupled with `FreeFlags`. If we have `template<FreeFlags
// flags> PartitionRoot::Free<flags>()` and the method needs a hint, the method
// must take `FreeHintType<FreeHintFlags(flags)>`.
template <FreeFlags free_flags>
struct FreeHint {
  typedef std::nullptr_t Type;
};

template <>
struct FreeHint<FreeFlags::kWithSizeHint> {
  struct Type {
    size_t size = 0;
  };
};

template <>
struct FreeHint<FreeFlags::kWithSizeHint | FreeFlags::kWithAlignmentHint> {
  struct Type {
    size_t size = 0;
    size_t alignment = 0;
  };
};

template <FreeFlags flags>
using FreeHintType = typename FreeHint<flags>::Type;

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_FREE_HINT_H_
