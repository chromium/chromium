// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_TO_VECTOR_H_
#define BASE_CONTAINERS_TO_VECTOR_H_

#include <algorithm>
#include <concepts>
#include <functional>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/types/cxx26_projected_value_t.h"

namespace base {

// Converts a container to a std::vector. The vector's element type is the same
// as the container's value type, if it is not explicitly specified.
//
// Complexity: linear in the size of `range`.
template <typename U = void,
          int&... ExplicitArgumentBarrier,
          typename Range,
          typename T = std::conditional_t<std::is_void_v<U>,
                                          std::ranges::range_value_t<Range>,
                                          U>>
  requires(std::ranges::input_range<Range>)
std::vector<T> ToVector(Range&& range) {
  return {std::from_range, std::forward<Range>(range)};
}

// Maps a container to a std::vector<> with respect to the provided projection.
// The deduced vector element type is equal to the projection's return type with
// cv-qualifiers removed if it's not explicitly specified.
//
// In C++23 this is roughly equal to:
// auto vec = range | std::views:transform(proj) | std::ranges::to<std::vector>;
//
// Complexity: Exactly `size(range)` applications of `proj`.
template <typename U = void,
          int&... ExplicitArgumentBarrier,
          typename Range,
          typename Proj,
          typename ProjectedType = std::conditional_t<
              std::is_void_v<U>,
              base::projected_value_t<std::ranges::iterator_t<Range>, Proj>,
              U>>
  requires std::ranges::sized_range<Range> && std::ranges::input_range<Range> &&
           std::indirectly_unary_invocable<Proj, std::ranges::iterator_t<Range>>
auto ToVector(Range&& range, Proj proj) {
  std::vector<ProjectedType> container;
  container.reserve(std::ranges::size(range));
  std::ranges::transform(std::forward<Range>(range),
                         std::back_inserter(container), std::move(proj));
  return container;
}

// Maps an rvalue array to a std::vector<>.
//
// This allows creating a std::vector<T> in a single expression, even when T is
// not copyable. For example, this doesn't work (because std::initializer_list
// provides only const access to the underlying array):
//
//     std::vector<std::unique_ptr<int>>{
//       std::make_unique<int>(17),
//       std::make_unique<int>(19),
//     }
//
// but this does:
//
//     base::ToVector({
//       std::make_unique<int>(17),
//       std::make_unique<int>(19),
//     })
//
// Similar API to C++20's std::to_array.
//
// Complexity: `N` move operations.
template <typename U = void,
          int&... ExplicitArgumentBarrier,
          typename T,
          size_t N,
          typename ResultType = std::conditional_t<std::is_void_v<U>, T, U>>
  requires(std::move_constructible<T>)
std::vector<ResultType> ToVector(T (&&array)[N]) {
  return {
      std::make_move_iterator(std::begin(array)),
      std::make_move_iterator(std::end(array)),
  };
}

}  // namespace base

#endif  // BASE_CONTAINERS_TO_VECTOR_H_
