// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_VARIANT_UTIL_H_
#define BASE_TYPES_VARIANT_UTIL_H_

#include <stddef.h>

#include <initializer_list>
#include <type_traits>
#include <variant>

namespace base {
namespace internal {

template <typename Variant, typename T>
struct VariantIndexOfTypeHelper {
  static_assert(false, "Variant must be an std::variant<...>");
};

template <typename... Ts, typename T>
struct VariantIndexOfTypeHelper<std::variant<Ts...>, T> {
  static constexpr size_t Index() {
    static_assert((0 + ... + std::is_same_v<T, Ts>) == 1,
                  "Variant is not constructible from T");
    size_t index = 0;
    for (bool is_t : {std::is_same_v<T, Ts>...}) {
      if (is_t) {
        break;
      }
      ++index;
    }
    return index;
  }
};

}  // namespace internal

// Returns the 0-based index of `T` in `Variant`'s list of alternative types,
// e.g. given `Variant` == `std::variant<A, B, C>` and `T` == `B`, returns 1.
//
// Note that this helper cannot be used if the list of alternative types
// contains duplicates.
template <typename Variant, typename T>
constexpr size_t VariantIndexOfType() {
  return internal::VariantIndexOfTypeHelper<Variant, T>::Index();
}

}  // namespace base

#endif  // BASE_TYPES_VARIANT_UTIL_H_
