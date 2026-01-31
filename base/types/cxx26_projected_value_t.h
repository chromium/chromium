// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_CXX26_PROJECTED_VALUE_T_H_
#define BASE_TYPES_CXX26_PROJECTED_VALUE_T_H_

#include <concepts>
#include <iterator>
#include <type_traits>

namespace base {

// C++26's std::projected_value_t.
//
// See https://en.cppreference.com/w/cpp/iterator/projected_value_t.html
template <typename I, typename Proj>
  requires(std::indirectly_readable<I> &&
           std::indirectly_regular_unary_invocable<Proj, I>)
using projected_value_t =
    std::remove_cvref_t<std::invoke_result_t<Proj&, std::iter_value_t<I>&>>;

}  // namespace base

#endif  // BASE_TYPES_CXX26_PROJECTED_VALUE_T_H_
