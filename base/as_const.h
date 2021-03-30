// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_AS_CONST_H_
#define BASE_AS_CONST_H_

#include <type_traits>

namespace base {

// C++14 implementation of C++17's std::as_const():
// https://en.cppreference.com/w/cpp/utility/as_const
template <typename T>
constexpr std::add_const_t<T>& as_const(T& t) noexcept {
  return t;
}

template <typename T>
void as_const(const T&& t) = delete;

}  // namespace base

#endif  // BASE_AS_CONST_H_
