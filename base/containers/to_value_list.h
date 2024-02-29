// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_TO_VALUE_LIST_H_
#define BASE_CONTAINERS_TO_VALUE_LIST_H_

#include <concepts>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/ranges/ranges.h"
#include "base/values.h"

namespace base {

namespace internal {
template <typename T>
concept AppendableToValueList =
    requires(T&& value) { Value::List().Append(std::forward<T>(value)); };
}  // namespace internal

// Maps a container to a Value::List with respect to the provided projection.
//
// Complexity: Exactly `size(range)` applications of `proj`.
template <typename Range, typename Proj = std::identity>
  requires requires { typename internal::range_category_t<Range>; } &&
           std::indirectly_unary_invocable<Proj, ranges::iterator_t<Range>> &&
           internal::AppendableToValueList<
               std::indirect_result_t<Proj, ranges::iterator_t<Range>>>
Value::List ToValueList(Range&& range, Proj proj = {}) {
  auto container = Value::List::with_capacity(std::size(range));
  ranges::for_each(
      std::forward<Range>(range),
      [&]<typename T>(T&& value) { container.Append(std::forward<T>(value)); },
      std::move(proj));
  return container;
}

}  // namespace base

#endif  // BASE_CONTAINERS_TO_VALUE_LIST_H_
