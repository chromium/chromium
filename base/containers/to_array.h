// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_TO_ARRAY_H_
#define BASE_CONTAINERS_TO_ARRAY_H_

#include <array>
#include <concepts>
#include <functional>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/types/cxx26_projected_value_t.h"

namespace base {

namespace to_array_internal {

// Returns the compile-time extent of `Range` if `Range` is a fixed-extent
// contiguous range (e.g. returning `N` for `T[N]`, `std::array<T, N>`, or
// `span<T, N>`), and `dynamic_extent` for dynamic or non-contiguous ranges.
template <typename Range>
inline constexpr size_t extent_of_v = [] {
  if constexpr (requires { base::span(std::declval<Range>()); }) {
    return decltype(base::span(std::declval<Range>()))::extent;
  } else {
    return dynamic_extent;
  }
}();

// Coerces `range` to a `base::span` if `range` is a contiguous range (e.g.
// C-style arrays, `std::array`, `std::vector`, `base::span`).
// Non-contiguous ranges or move-only rvalue views (e.g.
// `std::views::as_rvalue`) are passed through unchanged.
template <typename Range>
constexpr decltype(auto) ToSpanOrRange(Range&& range) {
  if constexpr (requires { base::span(std::forward<Range>(range)); }) {
    return base::span(std::forward<Range>(range));
  } else {
    return std::forward<Range>(range);
  }
}

template <typename ResultType,
          size_t N,
          typename Range,
          typename Proj,
          size_t... Is>
constexpr std::array<ResultType, N> ToArrayImpl(Range&& range,
                                                const Proj& proj,
                                                std::index_sequence<Is...>) {
  decltype(auto) span_or_range = ToSpanOrRange(std::forward<Range>(range));
  auto it = std::ranges::begin(span_or_range);
  return {{(void(Is), std::invoke(proj, *it++))...}};
}

}  // namespace to_array_internal

// Converts a container/span with compile-time fixed extent to a std::array.
// The array's element type is deduced from the container's value type (with
// cv-qualifiers removed) if it is not explicitly specified.
//
// Complexity: linear in the size of `range`.
template <typename U = void,
          int&... ExplicitArgumentBarrier,
          typename Range,
          size_t N = to_array_internal::extent_of_v<Range>,
          typename T = std::conditional_t<
              std::is_void_v<U>,
              std::remove_cv_t<std::ranges::range_value_t<Range>>,
              U>>
  requires(to_array_internal::extent_of_v<Range> != dynamic_extent) &&
          std::ranges::input_range<Range>
constexpr std::array<T, N> ToArray(Range&& range) {
  return to_array_internal::ToArrayImpl<T, N>(std::forward<Range>(range),
                                              std::identity{},
                                              std::make_index_sequence<N>{});
}

// Maps a container/span with compile-time fixed extent to a std::array with
// respect to the provided projection.
template <typename U = void,
          int&... ExplicitArgumentBarrier,
          typename Range,
          typename Proj,
          size_t N = to_array_internal::extent_of_v<Range>,
          typename ProjectedType = std::conditional_t<
              std::is_void_v<U>,
              base::projected_value_t<std::ranges::iterator_t<Range>, Proj>,
              U>>
  requires(to_array_internal::extent_of_v<Range> != dynamic_extent) &&
          std::ranges::input_range<Range> &&
          std::indirectly_unary_invocable<Proj, std::ranges::iterator_t<Range>>
constexpr std::array<ProjectedType, N> ToArray(Range&& range, Proj proj) {
  return to_array_internal::ToArrayImpl<ProjectedType, N>(
      std::forward<Range>(range), proj, std::make_index_sequence<N>{});
}

// Converts a container/span with dynamic or fixed extent to a std::array of
// size `N`. `N` must be specified as an explicit template argument.
template <size_t N,
          typename U = void,
          int&... ExplicitArgumentBarrier,
          typename Range,
          typename T = std::conditional_t<
              std::is_void_v<U>,
              std::remove_cv_t<std::ranges::range_value_t<Range>>,
              U>>
  requires std::ranges::sized_range<Range> &&
           (to_array_internal::extent_of_v<Range> == dynamic_extent ||
            to_array_internal::extent_of_v<Range> == N)
constexpr std::array<T, N> ToArray(Range&& range) {
  if constexpr (to_array_internal::extent_of_v<Range> == dynamic_extent) {
    CHECK(std::ranges::size(range) == N);
  }
  return to_array_internal::ToArrayImpl<T, N>(std::forward<Range>(range),
                                              std::identity{},
                                              std::make_index_sequence<N>{});
}

// Maps a container/span with dynamic or fixed extent to a std::array of size
// `N` with respect to the provided projection. `N` must be specified as an
// explicit template argument.
template <size_t N,
          typename U = void,
          int&... ExplicitArgumentBarrier,
          typename Range,
          typename Proj,
          typename ProjectedType = std::conditional_t<
              std::is_void_v<U>,
              base::projected_value_t<std::ranges::iterator_t<Range>, Proj>,
              U>>
  requires std::ranges::sized_range<Range> &&
           std::indirectly_unary_invocable<Proj,
                                           std::ranges::iterator_t<Range>> &&
           (to_array_internal::extent_of_v<Range> == dynamic_extent ||
            to_array_internal::extent_of_v<Range> == N)
constexpr std::array<ProjectedType, N> ToArray(Range&& range, Proj proj) {
  if constexpr (to_array_internal::extent_of_v<Range> == dynamic_extent) {
    CHECK(std::ranges::size(range) == N);
  }
  return to_array_internal::ToArrayImpl<ProjectedType, N>(
      std::forward<Range>(range), proj, std::make_index_sequence<N>{});
}

// Maps an rvalue array to a std::array.
// Similar to C++20's std::to_array.
template <typename U = void,
          int&... ExplicitArgumentBarrier,
          typename T,
          size_t N,
          typename ResultType =
              std::conditional_t<std::is_void_v<U>, std::remove_cv_t<T>, U>>
  requires std::move_constructible<T>
constexpr std::array<ResultType, N> ToArray(T (&&array)[N]) {
  return to_array_internal::ToArrayImpl<ResultType, N>(
      std::views::as_rvalue(base::span(array)), std::identity{},
      std::make_index_sequence<N>{});
}

}  // namespace base

#endif  // BASE_CONTAINERS_TO_ARRAY_H_
