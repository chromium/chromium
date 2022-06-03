// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CXX20_TO_ADDRESS_H_
#define BASE_CXX20_TO_ADDRESS_H_

#include <type_traits>

namespace base {

// Simplified C++14 implementation of C++20's std::to_address.
// Note: This does not consider specializations of pointer_traits<>::to_address,
// since that member function may only be present in C++20 and later.
//
// Reference: https://wg21.link/pointer.conversion#lib:to_address
template <typename T>
constexpr T* to_address(T* p) noexcept {
  static_assert(!std::is_function<T>::value,
                "Error: T must not be a function type.");
  return p;
}

template <typename Ptr>
constexpr auto to_address(const Ptr& p) noexcept {
  return to_address(p.operator->());
}

}  // namespace base

#endif  // BASE_CXX20_TO_ADDRESS_H_
