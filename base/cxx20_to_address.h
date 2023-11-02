// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CXX20_TO_ADDRESS_H_
#define BASE_CXX20_TO_ADDRESS_H_

#include <type_traits>

namespace base {

// Implementation of C++20's std::to_address.
// Note: This does consider specializations of pointer_traits<>::to_address,
// even though it's a C++20 member function, because CheckedContiguousIterator
// specializes pointer_traits<> with a to_address() member.
//
// Reference: https://wg21.link/pointer.conversion#lib:to_address
template <typename T>
constexpr T* to_address(T* p) noexcept {
  static_assert(!std::is_function<T>::value,
                "Error: T must not be a function type.");
  return p;
}

template <typename Ptr>
constexpr auto to_address(const Ptr& p) noexcept
    -> decltype(std::pointer_traits<Ptr>::to_address(p)) {
  return std::pointer_traits<Ptr>::to_address(p);
}

template <typename Ptr, typename... None>
constexpr auto to_address(const Ptr& p, None...) noexcept {
  return base::to_address(p.operator->());
}

}  // namespace base

#endif  // BASE_CXX20_TO_ADDRESS_H_
