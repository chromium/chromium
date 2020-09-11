// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RANGES_ITERATOR_H_
#define BASE_RANGES_ITERATOR_H_

#include <iterator>
#include <type_traits>
#include <utility>

#include "base/ranges/functional.h"

namespace base {

// Simplified implementation of C++20's std::iter_value_t.
// As opposed to std::iter_value_t, this implementation does not restrict
// the type of `Iter` and does not consider specializations of
// `indirectly_readable_traits`.
//
// Reference: https://wg21.link/readable.traits#2
template <typename Iter>
using iter_value_t = typename std::iterator_traits<
    std::remove_cv_t<std::remove_reference_t<Iter>>>::value_type;

// Simplified implementation of C++20's std::iter_reference_t.
// As opposed to std::iter_reference_t, this implementation does not restrict
// the type of `Iter`.
//
// Reference: https://wg21.link/iterator.synopsis#:~:text=iter_reference_t
template <typename Iter>
using iter_reference_t = decltype(*std::declval<Iter&>());

// Simplified implementation of C++20's std::indirect_result_t. As opposed to
// std::indirect_result_t, this implementation does not restrict the type of
// `Func` and `Iters`.
//
// Reference: https://wg21.link/iterator.synopsis#:~:text=indirect_result_t
template <typename Func, typename... Iters>
using indirect_result_t = invoke_result_t<Func, iter_reference_t<Iters>...>;

// Simplified implementation of C++20's std::projected. As opposed to
// std::projected, this implementation does not explicitly restrict the type of
// `Iter` and `Proj`, but rather does so implicitly by requiring
// `indirect_result_t<Proj, Iter>` is a valid type. This is required for SFINAE
// friendliness.
//
// Reference: https://wg21.link/projected
template <typename Iter,
          typename Proj,
          typename IndirectResultT = indirect_result_t<Proj, Iter>>
struct projected {
  using value_type = std::remove_cv_t<std::remove_reference_t<IndirectResultT>>;

  IndirectResultT operator*() const;  // not defined
};

}  // namespace base

#endif  // BASE_RANGES_ITERATOR_H_
