// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CXX17_BACKPORTS_H_
#define BASE_CXX17_BACKPORTS_H_

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/functional/invoke.h"

namespace base {

// C++14 implementation of C++17's std::clamp():
// https://en.cppreference.com/w/cpp/algorithm/clamp
// Please note that the C++ spec makes it undefined behavior to call std::clamp
// with a value of `lo` that compares greater than the value of `hi`. This
// implementation uses a CHECK to enforce this as a hard restriction.
template <typename T, typename Compare>
constexpr const T& clamp(const T& v, const T& lo, const T& hi, Compare comp) {
  CHECK(!comp(hi, lo));
  return comp(v, lo) ? lo : comp(hi, v) ? hi : v;
}

template <typename T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return base::clamp(v, lo, hi, std::less<T>{});
}

// C++14 implementation of C++17's std::apply():
// https://en.cppreference.com/w/cpp/utility/apply
namespace internal {
template <class F, class Tuple, std::size_t... I>
constexpr decltype(auto) apply_impl(F&& f,
                                    Tuple&& t,
                                    std::index_sequence<I...>) {
  return base::invoke(std::forward<F>(f),
                      std::get<I>(std::forward<Tuple>(t))...);
}
}  // namespace internal

template <class F, class Tuple>
constexpr decltype(auto) apply(F&& f, Tuple&& t) {
  return internal::apply_impl(
      std::forward<F>(f), std::forward<Tuple>(t),
      std::make_index_sequence<
          std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
}

}  // namespace base

#endif  // BASE_CXX17_BACKPORTS_H_
