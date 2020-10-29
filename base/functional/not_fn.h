// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUNCTIONAL_NOT_FN_H_
#define BASE_FUNCTIONAL_NOT_FN_H_

#include <type_traits>
#include <utility>

#include "base/functional/invoke.h"

namespace base {

namespace internal {

template <typename F>
struct NotFnImpl {
  F f;

  template <typename... Args>
  constexpr decltype(auto) operator()(Args&&... args) & noexcept {
    return !base::invoke(f, std::forward<Args>(args)...);
  }

  template <typename... Args>
  constexpr decltype(auto) operator()(Args&&... args) const& noexcept {
    return !base::invoke(f, std::forward<Args>(args)...);
  }

  template <typename... Args>
  constexpr decltype(auto) operator()(Args&&... args) && noexcept {
    return !base::invoke(std::move(f), std::forward<Args>(args)...);
  }

  template <typename... Args>
  constexpr decltype(auto) operator()(Args&&... args) const&& noexcept {
    return !base::invoke(std::move(f), std::forward<Args>(args)...);
  }
};

}  // namespace internal

// Implementation of C++17's std::not_fn.
//
// Reference:
// - https://en.cppreference.com/w/cpp/utility/functional/not_fn
// - https://wg21.link/func.not.fn
template <typename F>
constexpr internal::NotFnImpl<std::decay_t<F>> not_fn(F&& f) {
  return {std::forward<F>(f)};
}

}  // namespace base

#endif  // BASE_FUNCTIONAL_NOT_FN_H_
