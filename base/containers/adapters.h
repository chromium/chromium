// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_ADAPTERS_H_
#define BASE_CONTAINERS_ADAPTERS_H_

#include <stddef.h>

#include <iterator>
#include <utility>

#include "base/memory/raw_ref.h"

namespace base {

namespace internal {

// Internal adapter class for implementing base::Reversed.
template <typename T>
class ReversedAdapter {
 public:
  using Iterator = decltype(std::rbegin(std::declval<T&>()));

  explicit ReversedAdapter(T& t) : t_(t) {}
  ReversedAdapter(const ReversedAdapter& ra) : t_(ra.t_) {}
  ReversedAdapter& operator=(const ReversedAdapter&) = delete;

  Iterator begin() const { return std::rbegin(*t_); }
  Iterator end() const { return std::rend(*t_); }

 private:
  // `ReversedAdapter` and therefore `t_` are only used inside for loops. The
  // container being iterated over should be the one holding a raw_ref/raw_ptr
  // ideally. This member's type was rewritten into `const raw_ref` since it
  // didn't hurt binary size at the time of the rewrite.
  const raw_ref<T> t_;
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
