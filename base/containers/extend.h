// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_EXTEND_H_
#define BASE_CONTAINERS_EXTEND_H_

#include <iterator>
#include <vector>

#include "base/containers/span.h"

namespace base {

// Append to |dst| all elements of |src| by std::move-ing them out of |src|.
// After this operation, |src| will be empty.
template <typename T>
void Extend(std::vector<T>& dst, std::vector<T>&& src) {
  dst.insert(dst.end(), std::make_move_iterator(src.begin()),
             std::make_move_iterator(src.end()));
  src.clear();
}

// Append to |dst| all elements of |src| by copying them out of |src|. |src| is
// not changed.
template <typename T>
void Extend(std::vector<T>& dst, const std::vector<T>& src) {
  Extend(dst, make_span(src));
}

// Append to |dst| all elements of |src| by copying them out of |src|. |src| is
// not changed.
template <typename T, size_t N>
void Extend(std::vector<T>& dst, span<T, N> src) {
  dst.insert(dst.end(), src.begin(), src.end());
}

// Append to |dst| all elements of |src| by copying them out of |src|. |src| is
// not changed.
template <typename T, size_t N>
void Extend(std::vector<T>& dst, span<const T, N> src) {
  dst.insert(dst.end(), src.begin(), src.end());
}

// Append to |dst| all elements of |src| by copying them out of |src|. |src| is
// not changed.
//
// # Implementation note on convertible_to
// This overload allows implicit conversions to `span<T>`, in the same way that
// would occur if we received a non-template `span<int>`. This would not be
// possible by just receiving `span<T>` as the templated `T` can not be deduced
// (even though it is fixed by the deduction from the `vector<T>` parameter).
// The overloads above do not allow implicit conversion, but do accept
// fixed-size spans without losing the fixed-size `N`. They can not be written
// in the same format as the `N` would not be deducible for the `convertible_to`
// check.
template <typename T, typename S>
  requires(std::convertible_to<S, span<const T>>)
void Extend(std::vector<T>& dst, S&& src) {
  span<const T> src_span = src;
  dst.insert(dst.end(), src_span.begin(), src_span.end());
}

}  // namespace base

#endif  // BASE_CONTAINERS_EXTEND_H_
