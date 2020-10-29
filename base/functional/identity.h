// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUNCTIONAL_IDENTITY_H_
#define BASE_FUNCTIONAL_IDENTITY_H_

#include <utility>

namespace base {

// Implementation of C++20's std::identity.
//
// Reference:
// - https://en.cppreference.com/w/cpp/utility/functional/identity
// - https://wg21.link/func.identity
struct identity {
  template <typename T>
  constexpr T&& operator()(T&& t) const noexcept {
    return std::forward<T>(t);
  }

  using is_transparent = void;
};

}  // namespace base

#endif  // BASE_FUNCTIONAL_IDENTITY_H_
