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

namespace base {

// Maps a container to a std::vector<> with respect to the provided projection.
// The deduced vector element type is equal to the projection's return type with
// cv-qualifiers removed.
//
// In C++20 this is roughly equal to:
// auto vec = range | std::views:transform(proj) |
// std::ranges::to<std::vector>;
//
// Complexity: Exactly `size(range)` applications of `proj`.
template <typename Range, typename Proj = std::identity>
  requires std::ranges::sized_range<Range> && std::ranges::input_range<Range> &&
           std::indirectly_unary_invocable<Proj, std::ranges::iterator_t<Range>>
auto ToVector(Range&& range, Proj proj = {}) {
  using ProjectedType =
      std::indirectly_readable_traits<std::projected<std::ranges::iterator_t<Range>, Proj> >::value_type;
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
template <typename T, size_t N>
  requires(std::move_constructible<T>)
std::vector<T> ToVector(T (&&array)[N]) {
  return {
      std::make_move_iterator(std::begin(array)),
      std::make_move_iterator(std::end(array)),
  };
}

}  // namespace base

#endif  // BASE_CONTAINERS_TO_VECTOR_H_
