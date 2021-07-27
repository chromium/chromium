// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_RANGES_H_
#define BASE_NUMERICS_RANGES_H_

#include <algorithm>
#include <cmath>

namespace base {

// DO NOT USE THIS FUNCTION. IT IS DUE TO BE REMOVED. https://crbug.com/1231569
// Please use base::clamp() from base/cxx17_backports.h instead.
//
// This function, unlike base::clamp(), does not check if `min` is greater than
// `max`, and returns a bogus answer if it is. Please migrate all code that
// calls this function to use base::clamp() instead.
//
// If, for some reason the broken behavior is required, please re-create this
// min/max nesting inline in the host code and explain with a comment why it
// is needed.
template <class T>
constexpr const T& BrokenClampThatShouldNotBeUsed(const T& value,
                                                  const T& min,
                                                  const T& max) {
  return std::min(std::max(value, min), max);
}

template <typename T>
constexpr bool IsApproximatelyEqual(T lhs, T rhs, T tolerance) {
  static_assert(std::is_arithmetic<T>::value, "Argument must be arithmetic");
  return std::abs(rhs - lhs) <= tolerance;
}

}  // namespace base

#endif  // BASE_NUMERICS_RANGES_H_
