// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_LINEAR_FIND_H_
#define BASE_CONTAINERS_LINEAR_FIND_H_

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/types/cxx26_projected_value_t.h"

// Linear searches that return a pointer to the found element, or null.
//
// These always search linearly, like `std::ranges::find()`, even if the range
// has its own `find()`. For keyed lookups in maps and sets, use that `find()`
// or `base::FindOrNull()` from map_util.h instead.
//
// The result points into `range`, so ranges must be lvalues or borrowed views
// (e.g. `base::span`, `std::string_view`), and their elements must be stable
// lvalues, not temporaries or proxies. `LIFETIME_BOUND` catches common misuse,
// but not all of it.

namespace base {

namespace internal_linear_find {

// Ranges whose elements can be pointed to: stable (multi-pass) lvalues rather
// than temporaries or proxies.
template <typename R>
concept AddressableRange =
    std::ranges::forward_range<R&> &&
    std::is_lvalue_reference_v<std::ranges::range_reference_t<R&>>;

template <typename R, typename T, typename Proj>
concept FindableIn = AddressableRange<R> &&
                     std::indirect_binary_predicate<
                         std::ranges::equal_to,
                         std::projected<std::ranges::iterator_t<R&>, Proj>,
                         const T*>;

template <typename R, typename Pred, typename Proj>
concept FindableIfIn = AddressableRange<R> &&
                       std::indirect_unary_predicate<
                           Pred,
                           std::projected<std::ranges::iterator_t<R&>, Proj>>;

template <typename R, typename T, typename Proj>
constexpr auto* LinearFindImpl(R& range, const T& value, Proj proj) {
  auto it = std::ranges::find(range, value, std::move(proj));
  return it != std::ranges::end(range) ? std::addressof(*it) : nullptr;
}

template <typename R, typename Pred, typename Proj>
constexpr auto* LinearFindIfImpl(R& range, Pred pred, Proj proj) {
  auto it = std::ranges::find_if(range, std::move(pred), std::move(proj));
  return it != std::ranges::end(range) ? std::addressof(*it) : nullptr;
}

}  // namespace internal_linear_find

// Returns a pointer to the first element of `range` whose projection equals
// `value`, or null. For a map, the elements are the key-value pairs.
//
// Implementation note: borrowed views are taken by value in the overload
// below. For a by-value `[[gsl::Pointer]]` type like `base::span`,
// `LIFETIME_BOUND` ties the result to the viewed elements, whereas on a
// reference parameter it ties it to the view object itself, and then warns
// about safe code such as `return LinearFind(span_param, x);`.
// The `R&&` overload takes lvalue containers only: `borrowed_range<R>` rejects
// rvalues, and `!enable_borrowed_range` sends views to the by-value overload,
// which would otherwise be ambiguous for them.
template <int&... ExplicitArgumentBarrier,
          typename R,
          typename Proj = std::identity,
          typename T = projected_value_t<std::ranges::iterator_t<R&>, Proj>>
  requires std::ranges::borrowed_range<R> &&
           (!std::ranges::enable_borrowed_range<std::remove_cvref_t<R>>) &&
           internal_linear_find::FindableIn<R, T, Proj>
[[nodiscard]] constexpr auto* LinearFind(R&& range LIFETIME_BOUND,
                                         const T& value,
                                         Proj proj = {}) {
  return internal_linear_find::LinearFindImpl(range, value, std::move(proj));
}

template <int&... ExplicitArgumentBarrier,
          typename R,
          typename Proj = std::identity,
          typename T = projected_value_t<std::ranges::iterator_t<R&>, Proj>>
  requires std::ranges::borrowed_range<R> &&
           internal_linear_find::FindableIn<R, T, Proj>
[[nodiscard]] constexpr auto* LinearFind(R range LIFETIME_BOUND,
                                         const T& value,
                                         Proj proj = {}) {
  return internal_linear_find::LinearFindImpl(range, value, std::move(proj));
}

// Returns a pointer to the first element of `range` whose projection satisfies
// `pred`, or null.
template <int&... ExplicitArgumentBarrier,
          typename R,
          typename Pred,
          typename Proj = std::identity>
  requires std::ranges::borrowed_range<R> &&
           (!std::ranges::enable_borrowed_range<std::remove_cvref_t<R>>) &&
           internal_linear_find::FindableIfIn<R, Pred, Proj>
[[nodiscard]] constexpr auto* LinearFindIf(R&& range LIFETIME_BOUND,
                                           Pred pred,
                                           Proj proj = {}) {
  return internal_linear_find::LinearFindIfImpl(range, std::move(pred),
                                                std::move(proj));
}

template <int&... ExplicitArgumentBarrier,
          typename R,
          typename Pred,
          typename Proj = std::identity>
  requires std::ranges::borrowed_range<R> &&
           internal_linear_find::FindableIfIn<R, Pred, Proj>
[[nodiscard]] constexpr auto* LinearFindIf(R range LIFETIME_BOUND,
                                           Pred pred,
                                           Proj proj = {}) {
  return internal_linear_find::LinearFindIfImpl(range, std::move(pred),
                                                std::move(proj));
}

}  // namespace base

#endif  // BASE_CONTAINERS_LINEAR_FIND_H_
