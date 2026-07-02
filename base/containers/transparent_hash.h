// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_TRANSPARENT_HASH_H_
#define BASE_CONTAINERS_TRANSPARENT_HASH_H_

#include <stddef.h>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/containers/span.h"
#include "base/types/to_address.h"
#include "third_party/abseil-cpp/absl/hash/hash.h"

namespace base {
namespace internal_transparent_hash {

// 1. Base case: Implicitly convertible.
// Natively handles std::vector -> base::span and std::pair<std::vector> ->
// std::pair<base::span>.
template <typename View, typename Arg>
  requires std::convertible_to<const Arg&, View>
constexpr View ProjectToView(const Arg& arg) {
  return arg;
}

// 2. Pointer-like types.
// Natively handles unwrapping scoped_refptr, std::unique_ptr, std::shared_ptr,
// and raw_ptr to their underlying pointer type.
template <typename View, typename Arg>
  requires(!std::convertible_to<const Arg&, View> &&
           requires(const Arg& arg) {
             { base::to_address(arg) } -> std::convertible_to<View>;
           })
constexpr View ProjectToView(const Arg& arg) {
  return base::to_address(arg);
}

template <typename Arg, typename View>
concept ProjectableTo = requires(const Arg& arg) {
  internal_transparent_hash::ProjectToView<View>(arg);
};

template <typename T>
concept AbslHashable = requires(T val) { absl::HashOf(val); };

}  // namespace internal_transparent_hash

// A transparent hash functor that projects its argument to `T` before hashing.
template <typename T>
struct TransparentHashAs {
  static_assert(std::same_as<T, std::remove_cvref_t<T>>,
                "Transparent hash/equal view types must be completely "
                "unqualified value types.");
  static_assert(internal_transparent_hash::AbslHashable<T>,
                "View type must be hashable using absl::Hash.");
  using is_transparent = void;

  template <typename U>
    requires internal_transparent_hash::ProjectableTo<U, T>
  static size_t operator()(const U& value) {
    return absl::HashOf(internal_transparent_hash::ProjectToView<T>(value));
  }
};

// A transparent equality functor that projects both arguments to `T` before
// comparing.
template <typename T>
struct TransparentEqualAs {
  static_assert(std::same_as<T, std::remove_cvref_t<T>>,
                "Transparent hash/equal view types must be completely "
                "unqualified value types.");
  static_assert(std::equality_comparable<T>,
                "View type must be equality comparable.");
  using is_transparent = void;

  template <typename U1, typename U2>
    requires internal_transparent_hash::ProjectableTo<U1, T> &&
             internal_transparent_hash::ProjectableTo<U2, T>
  static constexpr bool operator()(const U1& lhs, const U2& rhs) {
    return internal_transparent_hash::ProjectToView<T>(lhs) ==
           internal_transparent_hash::ProjectToView<T>(rhs);
  }
};

}  // namespace base

#endif  // BASE_CONTAINERS_TRANSPARENT_HASH_H_
