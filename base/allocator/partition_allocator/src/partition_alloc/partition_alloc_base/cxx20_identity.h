// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif
#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_CXX20_IDENTITY_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_CXX20_IDENTITY_H_

#include <iterator>
#include <type_traits>

// An implementation of C++20 `std::identity` functor object.
//
// https://en.cppreference.com/w/cpp/utility/functional/identity.html

namespace partition_alloc::internal::base {

struct identity {
  template <typename T>
  constexpr T&& operator()(T&& t) const noexcept {
    return std::forward<T>(t);
  }

  using is_transparent = void;
};

}  // namespace partition_alloc::internal::base

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_CXX20_IDENTITY_H_
