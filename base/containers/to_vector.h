// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_TO_VECTOR_H_
#define BASE_CONTAINERS_TO_VECTOR_H_

#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/ranges/ranges.h"

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
  requires requires { typename internal::range_category_t<Range>; } &&
           std::indirectly_unary_invocable<Proj, ranges::iterator_t<Range>>
auto ToVector(Range&& range, Proj proj = {}) {
  using ProjectedType =
      std::projected<ranges::iterator_t<Range>, Proj>::value_type;
  std::vector<ProjectedType> container;
  container.reserve(std::size(range));
  ranges::transform(std::forward<Range>(range), std::back_inserter(container),
                    std::move(proj));
  return container;
}

}  // namespace base

#endif  // BASE_CONTAINERS_TO_VECTOR_H_
