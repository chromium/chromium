// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CXX17_BACKPORTS_H_
#define BASE_CXX17_BACKPORTS_H_

#include <array>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/functional/invoke.h"

namespace base {

// C++14 implementation of C++17's std::size():
// http://en.cppreference.com/w/cpp/iterator/size
template <typename Container>
constexpr auto size(const Container& c) -> decltype(c.size()) {
  return c.size();
}

template <typename T, size_t N>
constexpr size_t size(const T (&array)[N]) noexcept {
  return N;
}

// C++14 implementation of C++17's std::empty():
// http://en.cppreference.com/w/cpp/iterator/empty
template <typename Container>
constexpr auto empty(const Container& c) -> decltype(c.empty()) {
  return c.empty();
}

template <typename T, size_t N>
constexpr bool empty(const T (&array)[N]) noexcept {
  return false;
}

template <typename T>
constexpr bool empty(std::initializer_list<T> il) noexcept {
  return il.size() == 0;
}

// C++14 implementation of C++17's std::data():
// http://en.cppreference.com/w/cpp/iterator/data
template <typename Container>
constexpr auto data(Container& c) -> decltype(c.data()) {
  return c.data();
}

// std::basic_string::data() had no mutable overload prior to C++17 [1].
// Hence this overload is provided.
// Note: str[0] is safe even for empty strings, as they are guaranteed to be
// null-terminated [2].
//
// [1] http://en.cppreference.com/w/cpp/string/basic_string/data
// [2] http://en.cppreference.com/w/cpp/string/basic_string/operator_at
template <typename CharT, typename Traits, typename Allocator>
CharT* data(std::basic_string<CharT, Traits, Allocator>& str) {
  return std::addressof(str[0]);
}

template <typename Container>
constexpr auto data(const Container& c) -> decltype(c.data()) {
  return c.data();
}

template <typename T, size_t N>
constexpr T* data(T (&array)[N]) noexcept {
  return array;
}

template <typename T>
constexpr const T* data(std::initializer_list<T> il) noexcept {
  return il.begin();
}

// std::array::data() was not constexpr prior to C++17 [1].
// Hence these overloads are provided.
//
// [1] https://en.cppreference.com/w/cpp/container/array/data
template <typename T, size_t N>
constexpr T* data(std::array<T, N>& array) noexcept {
  return !array.empty() ? &array[0] : nullptr;
}

template <typename T, size_t N>
constexpr const T* data(const std::array<T, N>& array) noexcept {
  return !array.empty() ? &array[0] : nullptr;
}

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
