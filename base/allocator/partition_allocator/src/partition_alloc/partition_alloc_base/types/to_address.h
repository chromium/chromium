// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_TYPES_TO_ADDRESS_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_TYPES_TO_ADDRESS_H_

#include <memory>
#include <type_traits>

// SFINAE-compatible wrapper for `std::` `to_address()`.
//
// The standard does not require `std::` `to_address()` to be SFINAE-compatible
// when code attempts instantiation with non-pointer-like types, and libstdc++'s
// implementation hard errors. For the sake of templated code that wants simple,
// unified handling, this wrapper provides that guarantee. This allows code to
// use "`to_address()` would be valid here" as a constraint to detect
// pointer-like types.
namespace partition_alloc::internal::base {

template <typename T>
constexpr T* to_address(T* p) noexcept {
  static_assert(!std::is_function_v<T>,
                "to_address cannot be used on function pointers.");
  return p;
}

// These constraints cover the cases where `std::` `to_address()`'s fancy
// pointer overload is well-specified. Using the recursive internal call avoids
// non-SFINAE compatible standard library implementations.
template <typename P>
  requires requires(const P& p) { std::pointer_traits<P>::to_address(p); } ||
           requires(const P& p) { p.operator->(); }
constexpr auto to_address(const P& p) noexcept {
  if constexpr (requires(const P& f) {
                  { std::pointer_traits<P>::to_address(f) };
                }) {
    return std::pointer_traits<P>::to_address(p);
  } else {
    return partition_alloc::internal::base::to_address(p.operator->());
  }
}

}  // namespace partition_alloc::internal::base

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_TYPES_TO_ADDRESS_H_
