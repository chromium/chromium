// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CONTAINS_H_
#define BASE_CONTAINERS_CONTAINS_H_

// Provides `Contains()`, a general purpose utility to check whether a container
// contains a value. This will probe whether a `contains` or `find` member
// function on `container` exists, and fall back to a generic linear search over
// `container`.

#include <algorithm>
#include <concepts>
#include <ranges>
#include <utility>

namespace base {

// TODO(crbug.com/470391351): This is an alias to std::ranges::contains() now,
// migrate callers and delete.
template <typename Container, typename Value>
constexpr bool Contains(const Container& container, const Value& value) {
  if constexpr (requires {
                  { container.contains(value) } -> std::same_as<bool>;
                }) {
    static_assert(
        false, "Error: Use .contains() directly instead of base::Contains()");
  } else if constexpr (requires { container.find(value) != Container::npos; }) {
    static_assert(false,
                  "Error: Use .find() directly instead of base::Contains()");
  } else if constexpr (requires { container.find(value) != container.end(); }) {
    static_assert(false,
                  "Error: Use .find() directly instead of base::Contains()");
  } else {
    static_assert(
        !requires { typename Container::key_type; },
        "Error: About to perform linear search on an associative container. "
        "Either use a more generic comparator (e.g. std::less<>) or, if a "
        "linear search is desired, provide an explicit projection parameter.");
    return std::ranges::contains(container, value);
  }
}

// TODO(crbug.com/470391351): This is an alias to std::ranges::contains() now,
// migrate callers and delete.
template <typename Container, typename Value, typename Proj>
constexpr bool Contains(const Container& container,
                        const Value& value,
                        Proj proj) {
  return std::ranges::contains(container, value, std::move(proj));
}

}  // namespace base

#endif  // BASE_CONTAINERS_CONTAINS_H_
