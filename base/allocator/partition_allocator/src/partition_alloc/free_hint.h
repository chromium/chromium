// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_FREE_HINT_H_
#define PARTITION_ALLOC_FREE_HINT_H_

#include <concepts>
#include <cstddef>
#include <cstdint>

#include "partition_alloc/partition_alloc_base/types/strong_alias.h"

namespace partition_alloc {

using FreeSizeHint = internal::base::StrongAlias<class FreeSizeHintTag, size_t>;
using FreeAlignmentHint =
    internal::base::StrongAlias<class FreeAlignmentHintTag, size_t>;
using FreeTypeIdHint =
    internal::base::StrongAlias<class FreeTypeIdHintTag, uint32_t>;

template <typename T>
concept FreeHint =
    std::same_as<T, FreeSizeHint> || std::same_as<T, FreeAlignmentHint> ||
    std::same_as<T, FreeTypeIdHint>;

namespace internal {

template <FreeHint T, FreeHint... Hints>
inline constexpr bool kHasFreeHint = (std::same_as<T, Hints> || ...);

// Valid hint packs contain at most one of each `FreeHint` type, and
// `FreeAlignmentHint` is only used alongside `FreeSizeHint`.
template <FreeHint... Hints>
inline constexpr bool kAreValidFreeHints =
    (((std::same_as<FreeSizeHint, Hints> ? 1 : 0) + ... + 0) <= 1) &&
    (((std::same_as<FreeAlignmentHint, Hints> ? 1 : 0) + ... + 0) <= 1) &&
    (((std::same_as<FreeTypeIdHint, Hints> ? 1 : 0) + ... + 0) <= 1) &&
    (!kHasFreeHint<FreeAlignmentHint, Hints...> ||
     kHasFreeHint<FreeSizeHint, Hints...>);

template <FreeHint T, FreeHint First, FreeHint... Rest>
constexpr T GetFreeHint(First first, Rest... rest) {
  static_assert(kHasFreeHint<T, First, Rest...>);
  if constexpr (std::same_as<T, First>) {
    return first;
  } else {
    return GetFreeHint<T>(rest...);
  }
}

}  // namespace internal

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_FREE_HINT_H_
