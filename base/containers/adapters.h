// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_ADAPTERS_H_
#define BASE_CONTAINERS_ADAPTERS_H_

#include <stddef.h>

#include <iterator>
#include <ranges>
#include <utility>

#include "base/memory/raw_ptr_exclusion.h"

namespace base {

namespace internal {

// Internal adapter class for implementing base::Reversed.
// TODO(crbug.com/378623811): Parts of this (e.g. the `size()` helper) should be
// extracted to a base template that can be shared/reused. In addition, this
// should be constrained to Ts that satisfy the std::ranges::range concept.
template <typename T>
class ReversedAdapter {
 public:
  explicit ReversedAdapter(T& t) : t_(t) {}
  ReversedAdapter(const ReversedAdapter& ra) : t_(ra.t_) {}
  ReversedAdapter& operator=(const ReversedAdapter&) = delete;

  auto begin() { return std::rbegin(t_); }
  auto begin() const { return std::rbegin(t_); }
  auto cbegin() const { return std::crbegin(t_); }

  auto end() { return std::rend(t_); }
  auto end() const { return std::rend(t_); }
  auto cend() const { return std::crend(t_); }

  auto size() const
    requires std::ranges::sized_range<T>
  {
    return std::ranges::size(t_);
  }

 private:
  // RAW_PTR_EXCLUSION: References a STACK_ALLOCATED class. It is only used
  // inside for loops. Ideally, the container being iterated over should be the
  // one held via a raw_ref/raw_ptrs.
  RAW_PTR_EXCLUSION T& t_;
};

}  // namespace internal

// Reversed returns a container adapter usable in a range-based "for" statement
// for iterating a reversible container in reverse order.
//
// Example:
//
//   std::vector<int> v = ...;
//   for (int i : base::Reversed(v)) {
//     // iterates through v from back to front
//   }
template <typename T>
internal::ReversedAdapter<T> Reversed(T& t) {
  return internal::ReversedAdapter<T>(t);
}

}  // namespace base

#endif  // BASE_CONTAINERS_ADAPTERS_H_
