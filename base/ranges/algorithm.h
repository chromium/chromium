// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RANGES_ALGORITHM_H_
#define BASE_RANGES_ALGORITHM_H_

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/identity.h"
#include "base/functional/invoke.h"
#include "base/ranges/functional.h"
#include "base/ranges/ranges.h"
#include "base/template_util.h"

namespace base {

namespace internal {

// Returns a transformed version of the unary predicate `pred` applying `proj`
// to its argument before invoking `pred` on it.
// Ensures that the return type of `invoke(pred, ...)` is convertible to bool.
template <typename Pred, typename Proj>
constexpr auto ProjectedUnaryPredicate(Pred& pred, Proj& proj) noexcept {
  return [&pred, &proj](auto&& arg) -> bool {
    return base::invoke(pred,
                        base::invoke(proj, std::forward<decltype(arg)>(arg)));
  };
}

// Returns a transformed version of the binary predicate `pred` applying `proj1`
// and `proj2` to its arguments before invoking `pred` on them.
//
// Provides an opt-in to considers all four permutations of projections and
// argument types. This is sometimes necessary to allow usage with legacy
// non-ranges std:: algorithms that don't support projections.
//
// These permutations are assigned different priorities to break ambiguities in
// case several permutations are possible, e.g. when Proj1 and Proj2 are the
// same type.
//
// Note that even when opting in to using all permutations of projections,
// calling code should still ensure that the canonical mapping of {Proj1, Proj2}
// to {LHS, RHS} compiles for all members of the range. This can be done by
// adding the following constraint:
//
//   typename = indirect_result_t<Pred&,
//                                projected<iterator_t<Range1>, Proj1>,
//                                projected<iterator_t<Range2>, Proj2>>
//
// Ensures that the return type of `invoke(pred, ...)` is convertible to bool.
template <typename Pred, typename Proj1, typename Proj2, bool kPermute = false>
class BinaryPredicateProjector {
 public:
  constexpr BinaryPredicateProjector(Pred& pred, Proj1& proj1, Proj2& proj2)
      : pred_(pred), proj1_(proj1), proj2_(proj2) {}

 private:
  template <typename ProjT, typename ProjU, typename T, typename U>
  using InvokeResult = invoke_result_t<Pred&,
                                       invoke_result_t<ProjT&, T&&>,
                                       invoke_result_t<ProjU&, U&&>>;

  template <typename T, typename U, typename = InvokeResult<Proj1, Proj2, T, U>>
  constexpr std::pair<Proj1&, Proj2&> GetProjs(priority_tag<3>) const {
    return {proj1_, proj2_};
  }

  template <typename T,
            typename U,
            bool LazyPermute = kPermute,
            typename = std::enable_if_t<LazyPermute>,
            typename = InvokeResult<Proj2, Proj1, T, U>>
  constexpr std::pair<Proj2&, Proj1&> GetProjs(priority_tag<2>) const {
    return {proj2_, proj1_};
  }

  template <typename T,
            typename U,
            bool LazyPermute = kPermute,
            typename = std::enable_if_t<LazyPermute>,
            typename = InvokeResult<Proj1, Proj1, T, U>>
  constexpr std::pair<Proj1&, Proj1&> GetProjs(priority_tag<1>) const {
    return {proj1_, proj1_};
  }

  template <typename T,
            typename U,
            bool LazyPermute = kPermute,
            typename = std::enable_if_t<LazyPermute>,
            typename = InvokeResult<Proj2, Proj2, T, U>>
  constexpr std::pair<Proj2&, Proj2&> GetProjs(priority_tag<0>) const {
    return {proj2_, proj2_};
  }

 public:
  template <typename T, typename U>
  constexpr bool operator()(T&& lhs, U&& rhs) const {
    auto projs = GetProjs<T, U>(priority_tag<3>());
    return base::invoke(pred_, base::invoke(projs.first, std::forward<T>(lhs)),
                        base::invoke(projs.second, std::forward<U>(rhs)));
  }

 private:
  Pred& pred_;
  Proj1& proj1_;
  Proj2& proj2_;
};

// Small wrappers around BinaryPredicateProjector to make the calling side more
// readable.
template <typename Pred, typename Proj1, typename Proj2>
constexpr auto ProjectedBinaryPredicate(Pred& pred,
                                        Proj1& proj1,
                                        Proj2& proj2) noexcept {
  return BinaryPredicateProjector<Pred, Proj1, Proj2>(pred, proj1, proj2);
}

template <typename Pred, typename Proj1, typename Proj2>
constexpr auto PermutedProjectedBinaryPredicate(Pred& pred,
                                                Proj1& proj1,
                                                Proj2& proj2) noexcept {
  return BinaryPredicateProjector<Pred, Proj1, Proj2, true>(pred, proj1, proj2);
}

// This alias is used below to restrict iterator based APIs to types for which
// `iterator_category` and the pre-increment and post-increment operators are
// defined. This is required in situations where otherwise an undesired overload
// would be chosen, e.g. copy_if. In spirit this is similar to C++20's
// std::input_or_output_iterator, a concept that each iterator should satisfy.
template <typename Iter,
          typename = decltype(++std::declval<Iter&>()),
          typename = decltype(std::declval<Iter&>()++)>
using iterator_category_t =
    typename std::iterator_traits<Iter>::iterator_category;

// This alias is used below to restrict range based APIs to types for which
// `iterator_category_t` is defined for the underlying iterator. This is
// required in situations where otherwise an undesired overload would be chosen,
// e.g. transform. In spirit this is similar to C++20's std::ranges::range, a
// concept that each range should satisfy.
template <typename Range>
using range_category_t = iterator_category_t<ranges::iterator_t<Range>>;

}  // namespace internal

namespace ranges {

// C++14 implementation of std::ranges::in_fun_result.
//
// Reference: https://wg21.link/algorithms.results#:~:text=in_fun_result
template <typename I, typename F>
struct in_fun_result {
  NO_UNIQUE_ADDRESS I in;
  NO_UNIQUE_ADDRESS F fun;

  template <typename I2,
            typename F2,
            std::enable_if_t<std::is_convertible<const I&, I2>{} &&
                             std::is_convertible<const F&, F2>{}>>
  constexpr operator in_fun_result<I2, F2>() const& {
    return {in, fun};
  }

  template <typename I2,
            typename F2,
            std::enable_if_t<std::is_convertible<I, I2>{} &&
                             std::is_convertible<F, F2>{}>>
  constexpr operator in_fun_result<I2, F2>() && {
    return {std::move(in), std::move(fun)};
  }
};

// TODO(crbug.com/1071094): Implement the other result types.

// [alg.nonmodifying] Non-modifying sequence operations
// Reference: https://wg21.link/alg.nonmodifying

// [alg.all.of] All of
// Reference: https://wg21.link/alg.all.of

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `false` if `E(i)` is `false` for some iterator `i` in the range
// `[first, last)`, and `true` otherwise.
//
// Complexity: At most `last - first` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.all.of#:~:text=ranges::all_of(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr bool all_of(InputIterator first,
                      InputIterator last,
                      Pred pred,
                      Proj proj = {}) {
  for (; first != last; ++first) {
    if (!base::invoke(pred, base::invoke(proj, *first)))
      return false;
  }

  return true;
}

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `false` if `E(i)` is `false` for some iterator `i` in `range`, and
// `true` otherwise.
//
// Complexity: At most `size(range)` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.all.of#:~:text=ranges::all_of(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr bool all_of(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::all_of(ranges::begin(range), ranges::end(range),
                        std::move(pred), std::move(proj));
}

// [alg.any.of] Any of
// Reference: https://wg21.link/alg.any.of

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `true` if `E(i)` is `true` for some iterator `i` in the range
// `[first, last)`, and `false` otherwise.
//
// Complexity: At most `last - first` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.any.of#:~:text=ranges::any_of(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr bool any_of(InputIterator first,
                      InputIterator last,
                      Pred pred,
                      Proj proj = {}) {
  for (; first != last; ++first) {
    if (base::invoke(pred, base::invoke(proj, *first)))
      return true;
  }

  return false;
}

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `true` if `E(i)` is `true` for some iterator `i` in `range`, and
// `false` otherwise.
//
// Complexity: At most `size(range)` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.any.of#:~:text=ranges::any_of(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr bool any_of(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::any_of(ranges::begin(range), ranges::end(range),
                        std::move(pred), std::move(proj));
}

// [alg.none.of] None of
// Reference: https://wg21.link/alg.none.of

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `false` if `E(i)` is `true` for some iterator `i` in the range
// `[first, last)`, and `true` otherwise.
//
// Complexity: At most `last - first` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.none.of#:~:text=ranges::none_of(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr bool none_of(InputIterator first,
                       InputIterator last,
                       Pred pred,
                       Proj proj = {}) {
  for (; first != last; ++first) {
    if (base::invoke(pred, base::invoke(proj, *first)))
      return false;
  }

  return true;
}

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `false` if `E(i)` is `true` for some iterator `i` in `range`, and
// `true` otherwise.
//
// Complexity: At most `size(range)` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.none.of#:~:text=ranges::none_of(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr bool none_of(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::none_of(ranges::begin(range), ranges::end(range),
                         std::move(pred), std::move(proj));
}

// [alg.foreach] For each
// Reference: https://wg21.link/alg.foreach

// Reference: https://wg21.link/algorithm.syn#:~:text=for_each_result
template <typename I, typename F>
using for_each_result = in_fun_result<I, F>;

// Effects: Calls `invoke(f, invoke(proj, *i))` for every iterator `i` in the
// range `[first, last)`, starting from `first` and proceeding to `last - 1`.
//
// Returns: `{last, std::move(f)}`.
//
// Complexity: Applies `f` and `proj` exactly `last - first` times.
//
// Remarks: If `f` returns a result, the result is ignored.
//
// Reference: https://wg21.link/alg.foreach#:~:text=ranges::for_each(I
template <typename InputIterator,
          typename Fun,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto for_each(InputIterator first,
                        InputIterator last,
                        Fun f,
                        Proj proj = {}) {
  for (; first != last; ++first)
    base::invoke(f, base::invoke(proj, *first));
  return for_each_result<InputIterator, Fun>{first, std::move(f)};
}

// Effects: Calls `invoke(f, invoke(proj, *i))` for every iterator `i` in the
// range `range`, starting from `begin(range)` and proceeding to `end(range) -
// 1`.
//
// Returns: `{last, std::move(f)}`.
//
// Complexity: Applies `f` and `proj` exactly `size(range)` times.
//
// Remarks: If `f` returns a result, the result is ignored.
//
// Reference: https://wg21.link/alg.foreach#:~:text=ranges::for_each(R
template <typename Range,
          typename Fun,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto for_each(Range&& range, Fun f, Proj proj = {}) {
  return ranges::for_each(ranges::begin(range), ranges::end(range),
                          std::move(f), std::move(proj));
}

// Reference: https://wg21.link/algorithm.syn#:~:text=for_each_n_result
template <typename I, typename F>
using for_each_n_result = in_fun_result<I, F>;

// Preconditions: `n >= 0` is `true`.
//
// Effects: Calls `invoke(f, invoke(proj, *i))` for every iterator `i` in the
// range `[first, first + n)` in order.
//
// Returns: `{first + n, std::move(f)}`.
//
// Remarks: If `f` returns a result, the result is ignored.
//
// Reference: https://wg21.link/alg.foreach#:~:text=ranges::for_each_n
template <typename InputIterator,
          typename Size,
          typename Fun,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto for_each_n(InputIterator first, Size n, Fun f, Proj proj = {}) {
  while (n > 0) {
    base::invoke(f, base::invoke(proj, *first));
    ++first;
    --n;
  }

  return for_each_n_result<InputIterator, Fun>{first, std::move(f)};
}

// [alg.find] Find
// Reference: https://wg21.link/alg.find

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Returns: The first iterator `i` in the range `[first, last)` for which `E(i)`
// is `true`. Returns `last` if no such iterator is found.
//
// Complexity: At most `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find(I
template <typename InputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto find(InputIterator first,
                    InputIterator last,
                    const T& value,
                    Proj proj = {}) {
  for (; first != last; ++first) {
    if (base::invoke(proj, *first) == value)
      break;
  }

  return first;
}

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Returns: The first iterator `i` in `range` for which `E(i)` is `true`.
// Returns `end(range)` if no such iterator is found.
//
// Complexity: At most `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find(R
template <typename Range,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto find(Range&& range, const T& value, Proj proj = {}) {
  return ranges::find(ranges::begin(range), ranges::end(range), value,
                      std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Returns: The first iterator `i` in the range `[first, last)` for which `E(i)`
// is `true`. Returns `last` if no such iterator is found.
//
// Complexity: At most `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find_if(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto find_if(InputIterator first,
                       InputIterator last,
                       Pred pred,
                       Proj proj = {}) {
  return std::find_if(first, last,
                      internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Returns: The first iterator `i` in `range` for which `E(i)` is `true`.
// Returns `end(range)` if no such iterator is found.
//
// Complexity: At most `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find_if(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto find_if(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::find_if(ranges::begin(range), ranges::end(range),
                         std::move(pred), std::move(proj));
}

// Let `E(i)` be `bool(!invoke(pred, invoke(proj, *i)))`.
//
// Returns: The first iterator `i` in the range `[first, last)` for which `E(i)`
// is `true`. Returns `last` if no such iterator is found.
//
// Complexity: At most `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find_if_not(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto find_if_not(InputIterator first,
                           InputIterator last,
                           Pred pred,
                           Proj proj = {}) {
  return std::find_if_not(first, last,
                          internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(!invoke(pred, invoke(proj, *i)))`.
//
// Returns: The first iterator `i` in `range` for which `E(i)` is `true`.
// Returns `end(range)` if no such iterator is found.
//
// Complexity: At most `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find_if_not(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto find_if_not(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::find_if_not(ranges::begin(range), ranges::end(range),
                             std::move(pred), std::move(proj));
}

// [alg.find.end] Find end
// Reference: https://wg21.link/alg.find.end

// Let:
// - `E(i,n)` be `invoke(pred, invoke(proj1, *(i + n)),
//                             invoke(proj2, *(first2 + n)))`
//
// - `i` be `last1` if `[first2, last2)` is empty, or if
//   `(last2 - first2) > (last1 - first1)` is `true`, or if there is no iterator
//   in the range `[first1, last1 - (last2 - first2))` such that for every
//   non-negative integer `n < (last2 - first2)`, `E(i,n)` is `true`. Otherwise
//   `i` is the last such iterator in `[first1, last1 - (last2 - first2))`.
//
// Returns: `i`
// Note: std::ranges::find_end(I1 first1,...) returns a range, rather than an
// iterator. For simplicitly we match std::find_end's return type instead.
//
// Complexity:
// At most `(last2 - first2) * (last1 - first1 - (last2 - first2) + 1)`
// applications of the corresponding predicate and any projections.
//
// Reference: https://wg21.link/alg.find.end#:~:text=ranges::find_end(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>,
          typename = indirect_result_t<Pred&,
                                       projected<ForwardIterator1, Proj1>,
                                       projected<ForwardIterator2, Proj2>>>
constexpr auto find_end(ForwardIterator1 first1,
                        ForwardIterator1 last1,
                        ForwardIterator2 first2,
                        ForwardIterator2 last2,
                        Pred pred = {},
                        Proj1 proj1 = {},
                        Proj2 proj2 = {}) {
  return std::find_end(first1, last1, first2, last2,
                       internal::ProjectedBinaryPredicate(pred, proj1, proj2));
}

// Let:
// - `E(i,n)` be `invoke(pred, invoke(proj1, *(i + n)),
//                             invoke(proj2, *(first2 + n)))`
//
// - `i` be `end(range1)` if `range2` is empty, or if
//   `size(range2) > size(range1)` is `true`, or if there is no iterator in the
//   range `[begin(range1), end(range1) - size(range2))` such that for every
//   non-negative integer `n < size(range2)`, `E(i,n)` is `true`. Otherwise `i`
//   is the last such iterator in `[begin(range1), end(range1) - size(range2))`.
//
// Returns: `i`
// Note: std::ranges::find_end(R1&& r1,...) returns a range, rather than an
// iterator. For simplicitly we match std::find_end's return type instead.
//
// Complexity: At most `size(range2) * (size(range1) - size(range2) + 1)`
// applications of the corresponding predicate and any projections.
//
// Reference: https://wg21.link/alg.find.end#:~:text=ranges::find_end(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = indirect_result_t<Pred&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>>
constexpr auto find_end(Range1&& range1,
                        Range2&& range2,
                        Pred pred = {},
                        Proj1 proj1 = {},
                        Proj2 proj2 = {}) {
  return ranges::find_end(ranges::begin(range1), ranges::end(range1),
                          ranges::begin(range2), ranges::end(range2),
                          std::move(pred), std::move(proj1), std::move(proj2));
}

// [alg.find.first.of] Find first
// Reference: https://wg21.link/alg.find.first.of

// Let `E(i,j)` be `bool(invoke(pred, invoke(proj1, *i), invoke(proj2, *j)))`.
//
// Effects: Finds an element that matches one of a set of values.
//
// Returns: The first iterator `i` in the range `[first1, last1)` such that for
// some iterator `j` in the range `[first2, last2)` `E(i,j)` holds. Returns
// `last1` if `[first2, last2)` is empty or if no such iterator is found.
//
// Complexity: At most `(last1 - first1) * (last2 - first2)` applications of the
// corresponding predicate and any projections.
//
// Reference:
// https://wg21.link/alg.find.first.of#:~:text=ranges::find_first_of(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>,
          typename = indirect_result_t<Pred&,
                                       projected<ForwardIterator1, Proj1>,
                                       projected<ForwardIterator2, Proj2>>>
constexpr auto find_first_of(ForwardIterator1 first1,
                             ForwardIterator1 last1,
                             ForwardIterator2 first2,
                             ForwardIterator2 last2,
                             Pred pred = {},
                             Proj1 proj1 = {},
                             Proj2 proj2 = {}) {
  return std::find_first_of(
      first1, last1, first2, last2,
      internal::ProjectedBinaryPredicate(pred, proj1, proj2));
}

// Let `E(i,j)` be `bool(invoke(pred, invoke(proj1, *i), invoke(proj2, *j)))`.
//
// Effects: Finds an element that matches one of a set of values.
//
// Returns: The first iterator `i` in `range1` such that for some iterator `j`
// in `range2` `E(i,j)` holds. Returns `end(range1)` if `range2` is empty or if
// no such iterator is found.
//
// Complexity: At most `size(range1) * size(range2)` applications of the
// corresponding predicate and any projections.
//
// Reference:
// https://wg21.link/alg.find.first.of#:~:text=ranges::find_first_of(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = indirect_result_t<Pred&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>>
constexpr auto find_first_of(Range1&& range1,
                             Range2&& range2,
                             Pred pred = {},
                             Proj1 proj1 = {},
                             Proj2 proj2 = {}) {
  return ranges::find_first_of(
      ranges::begin(range1), ranges::end(range1), ranges::begin(range2),
      ranges::end(range2), std::move(pred), std::move(proj1), std::move(proj2));
}

// [alg.adjacent.find] Adjacent find
// Reference: https://wg21.link/alg.adjacent.find

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i), invoke(proj, *(i + 1))))`.
//
// Returns: The first iterator `i` such that both `i` and `i + 1` are in the
// range `[first, last)` for which `E(i)` holds. Returns `last` if no such
// iterator is found.
//
// Complexity: Exactly `min((i - first) + 1, (last - first) - 1)` applications
// of the corresponding predicate, where `i` is `adjacent_find`'s return value.
//
// Reference:
// https://wg21.link/alg.adjacent.find#:~:text=ranges::adjacent_find(I
template <typename ForwardIterator,
          typename Pred = ranges::equal_to,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto adjacent_find(ForwardIterator first,
                             ForwardIterator last,
                             Pred pred = {},
                             Proj proj = {}) {
  // Implementation inspired by cppreference.com:
  // https://en.cppreference.com/w/cpp/algorithm/adjacent_find
  //
  // A reimplementation is required, because std::adjacent_find is not constexpr
  // prior to C++20. Once we have C++20, we should switch to standard library
  // implementation.
  if (first == last)
    return last;

  for (ForwardIterator next = first; ++next != last; ++first) {
    if (base::invoke(pred, base::invoke(proj, *first),
                     base::invoke(proj, *next))) {
      return first;
    }
  }

  return last;
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i), invoke(proj, *(i + 1))))`.
//
// Returns: The first iterator `i` such that both `i` and `i + 1` are in the
// range `range` for which `E(i)` holds. Returns `end(range)` if no such
// iterator is found.
//
// Complexity: Exactly `min((i - begin(range)) + 1, size(range) - 1)`
// applications of the corresponding predicate, where `i` is `adjacent_find`'s
// return value.
//
// Reference:
// https://wg21.link/alg.adjacent.find#:~:text=ranges::adjacent_find(R
template <typename Range,
          typename Pred = ranges::equal_to,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto adjacent_find(Range&& range, Pred pred = {}, Proj proj = {}) {
  return ranges::adjacent_find(ranges::begin(range), ranges::end(range),
                               std::move(pred), std::move(proj));
}

// [alg.count] Count
// Reference: https://wg21.link/alg.count

// Let `E(i)` be `invoke(proj, *i) == value`.
//
// Effects: Returns the number of iterators `i` in the range `[first, last)` for
// which `E(i)` holds.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.count#:~:text=ranges::count(I
template <typename InputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto count(InputIterator first,
                     InputIterator last,
                     const T& value,
                     Proj proj = {}) {
  // Note: In order to be able to apply `proj` to each element in [first, last)
  // we are dispatching to std::count_if instead of std::count.
  return std::count_if(first, last, [&proj, &value](auto&& lhs) {
    return base::invoke(proj, std::forward<decltype(lhs)>(lhs)) == value;
  });
}

// Let `E(i)` be `invoke(proj, *i) == value`.
//
// Effects: Returns the number of iterators `i` in `range` for which `E(i)`
// holds.
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.count#:~:text=ranges::count(R
template <typename Range,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto count(Range&& range, const T& value, Proj proj = {}) {
  return ranges::count(ranges::begin(range), ranges::end(range), value,
                       std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Effects: Returns the number of iterators `i` in the range `[first, last)` for
// which `E(i)` holds.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.count#:~:text=ranges::count_if(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto count_if(InputIterator first,
                        InputIterator last,
                        Pred pred,
                        Proj proj = {}) {
  return std::count_if(first, last,
                       internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Effects: Returns the number of iterators `i` in `range` for which `E(i)`
// holds.
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.count#:~:text=ranges::count_if(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto count_if(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::count_if(ranges::begin(range), ranges::end(range),
                          std::move(pred), std::move(proj));
}

// [mismatch] Mismatch
// Reference: https://wg21.link/mismatch

// Let `E(n)` be `!invoke(pred, invoke(proj1, *(first1 + n)),
//                              invoke(proj2, *(first2 + n)))`.
//
// Let `N` be `min(last1 - first1, last2 - first2)`.
//
// Returns: `{ first1 + n, first2 + n }`, where `n` is the smallest integer in
// `[0, N)` such that `E(n)` holds, or `N` if no such integer exists.
//
// Complexity: At most `N` applications of the corresponding predicate and any
// projections.
//
// Reference: https://wg21.link/mismatch#:~:text=ranges::mismatch(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>,
          typename = indirect_result_t<Pred&,
                                       projected<ForwardIterator1, Proj1>,
                                       projected<ForwardIterator2, Proj2>>>
constexpr auto mismatch(ForwardIterator1 first1,
                        ForwardIterator1 last1,
                        ForwardIterator2 first2,
                        ForwardIterator2 last2,
                        Pred pred = {},
                        Proj1 proj1 = {},
                        Proj2 proj2 = {}) {
  return std::mismatch(first1, last1, first2, last2,
                       internal::ProjectedBinaryPredicate(pred, proj1, proj2));
}

// Let `E(n)` be `!invoke(pred, invoke(proj1, *(begin(range1) + n)),
//                              invoke(proj2, *(begin(range2) + n)))`.
//
// Let `N` be `min(size(range1), size(range2))`.
//
// Returns: `{ begin(range1) + n, begin(range2) + n }`, where `n` is the
// smallest integer in `[0, N)` such that `E(n)` holds, or `N` if no such
// integer exists.
//
// Complexity: At most `N` applications of the corresponding predicate and any
// projections.
//
// Reference: https://wg21.link/mismatch#:~:text=ranges::mismatch(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = indirect_result_t<Pred&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>>
constexpr auto mismatch(Range1&& range1,
                        Range2&& range2,
                        Pred pred = {},
                        Proj1 proj1 = {},
                        Proj2 proj2 = {}) {
  return ranges::mismatch(ranges::begin(range1), ranges::end(range1),
                          ranges::begin(range2), ranges::end(range2),
                          std::move(pred), std::move(proj1), std::move(proj2));
}

// [alg.equal] Equal
// Reference: https://wg21.link/alg.equal

// Let `E(i)` be
//   `invoke(pred, invoke(proj1, *i), invoke(proj2, *(first2 + (i - first1))))`.
//
// Returns: If `last1 - first1 != last2 - first2`, return `false.` Otherwise
// return `true` if `E(i)` holds for every iterator `i` in the range `[first1,
// last1)`. Otherwise, returns `false`.
//
// Complexity: If the types of `first1`, `last1`, `first2`, and `last2` meet the
// `RandomAccessIterator` requirements and `last1 - first1 != last2 - first2`,
// then no applications of the corresponding predicate and each projection;
// otherwise, at most `min(last1 - first1, last2 - first2)` applications of the
// corresponding predicate and any projections.
//
// Reference: https://wg21.link/alg.equal#:~:text=ranges::equal(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>,
          typename = indirect_result_t<Pred&,
                                       projected<ForwardIterator1, Proj1>,
                                       projected<ForwardIterator2, Proj2>>>
constexpr bool equal(ForwardIterator1 first1,
                     ForwardIterator1 last1,
                     ForwardIterator2 first2,
                     ForwardIterator2 last2,
                     Pred pred = {},
                     Proj1 proj1 = {},
                     Proj2 proj2 = {}) {
  if (base::is_constant_evaluated()) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
      if (!base::invoke(pred, base::invoke(proj1, *first1),
                        base::invoke(proj2, *first2))) {
        return false;
      }
    }

    return first1 == last1 && first2 == last2;
  }

  return std::equal(first1, last1, first2, last2,
                    internal::ProjectedBinaryPredicate(pred, proj1, proj2));
}

// Let `E(i)` be
//   `invoke(pred, invoke(proj1, *i),
//                 invoke(proj2, *(begin(range2) + (i - begin(range1)))))`.
//
// Returns: If `size(range1) != size(range2)`, return `false.` Otherwise return
// `true` if `E(i)` holds for every iterator `i` in `range1`. Otherwise, returns
// `false`.
//
// Complexity: If the types of `begin(range1)`, `end(range1)`, `begin(range2)`,
// and `end(range2)` meet the `RandomAccessIterator` requirements and
// `size(range1) != size(range2)`, then no applications of the corresponding
// predicate and each projection;
// otherwise, at most `min(size(range1), size(range2))` applications of the
// corresponding predicate and any projections.
//
// Reference: https://wg21.link/alg.equal#:~:text=ranges::equal(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = indirect_result_t<Pred&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>>
constexpr bool equal(Range1&& range1,
                     Range2&& range2,
                     Pred pred = {},
                     Proj1 proj1 = {},
                     Proj2 proj2 = {}) {
  return ranges::equal(ranges::begin(range1), ranges::end(range1),
                       ranges::begin(range2), ranges::end(range2),
                       std::move(pred), std::move(proj1), std::move(proj2));
}

// [alg.is.permutation] Is permutation
// Reference: https://wg21.link/alg.is.permutation

// Returns: If `last1 - first1 != last2 - first2`, return `false`. Otherwise
// return `true` if there exists a permutation of the elements in the range
// `[first2, last2)`, bounded by `[pfirst, plast)`, such that
// `ranges::equal(first1, last1, pfirst, plast, pred, proj, proj)` returns
// `true`; otherwise, returns `false`.
//
// Complexity: No applications of the corresponding predicate if
// ForwardIterator1 and ForwardIterator2 meet the requirements of random access
// iterators and `last1 - first1 != last2 - first2`. Otherwise, exactly
// `last1 - first1` applications of the corresponding predicate and projections
// if `ranges::equal(first1, last1, first2, last2, pred, proj, proj)` would
// return true;
// otherwise, at worst `O(N^2)`, where `N` has the value `last1 - first1`.
//
// Reference:
// https://wg21.link/alg.is.permutation#:~:text=ranges::is_permutation(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>,
          typename = indirect_result_t<Pred&,
                                       projected<ForwardIterator1, Proj1>,
                                       projected<ForwardIterator2, Proj2>>>
constexpr bool is_permutation(ForwardIterator1 first1,
                              ForwardIterator1 last1,
                              ForwardIterator2 first2,
                              ForwardIterator2 last2,
                              Pred pred = {},
                              Proj1 proj1 = {},
                              Proj2 proj2 = {}) {
  // Needs to opt-in to all permutations, since std::is_permutation expects
  // pred(proj1(lhs), proj1(rhs)) to compile.
  return std::is_permutation(
      first1, last1, first2, last2,
      internal::PermutedProjectedBinaryPredicate(pred, proj1, proj2));
}

// Returns: If `size(range1) != size(range2)`, return `false`. Otherwise return
// `true` if there exists a permutation of the elements in `range2`, bounded by
// `[pbegin, pend)`, such that
// `ranges::equal(range1, [pbegin, pend), pred, proj, proj)` returns `true`;
// otherwise, returns `false`.
//
// Complexity: No applications of the corresponding predicate if Range1 and
// Range2 meet the requirements of random access ranges and
// `size(range1) != size(range2)`. Otherwise, exactly `size(range1)`
// applications of the corresponding predicate and projections if
// `ranges::equal(range1, range2, pred, proj, proj)` would return true;
// otherwise, at worst `O(N^2)`, where `N` has the value `size(range1)`.
//
// Reference:
// https://wg21.link/alg.is.permutation#:~:text=ranges::is_permutation(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = indirect_result_t<Pred&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>>
constexpr bool is_permutation(Range1&& range1,
                              Range2&& range2,
                              Pred pred = {},
                              Proj1 proj1 = {},
                              Proj2 proj2 = {}) {
  return ranges::is_permutation(
      ranges::begin(range1), ranges::end(range1), ranges::begin(range2),
      ranges::end(range2), std::move(pred), std::move(proj1), std::move(proj2));
}

// [alg.search] Search
// Reference: https://wg21.link/alg.search

// Returns: `i`, where `i` is the first iterator in the range
// `[first1, last1 - (last2 - first2))` such that for every non-negative integer
// `n` less than `last2 - first2` the condition
// `bool(invoke(pred, invoke(proj1, *(i + n)), invoke(proj2, *(first2 + n))))`
// is `true`.
// Returns `last1` if no such iterator exists.
// Note: std::ranges::search(I1 first1,...) returns a range, rather than an
// iterator. For simplicitly we match std::search's return type instead.
//
// Complexity: At most `(last1 - first1) * (last2 - first2)` applications of the
// corresponding predicate and projections.
//
// Reference: https://wg21.link/alg.search#:~:text=ranges::search(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>,
          typename = indirect_result_t<Pred&,
                                       projected<ForwardIterator1, Proj1>,
                                       projected<ForwardIterator2, Proj2>>>
constexpr auto search(ForwardIterator1 first1,
                      ForwardIterator1 last1,
                      ForwardIterator2 first2,
                      ForwardIterator2 last2,
                      Pred pred = {},
                      Proj1 proj1 = {},
                      Proj2 proj2 = {}) {
  return std::search(first1, last1, first2, last2,
                     internal::ProjectedBinaryPredicate(pred, proj1, proj2));
}

// Returns: `i`, where `i` is the first iterator in the range
// `[begin(range1), end(range1) - size(range2))` such that for every
// non-negative integer `n` less than `size(range2)` the condition
// `bool(invoke(pred, invoke(proj1, *(i + n)),
//                    invoke(proj2, *(begin(range2) + n))))` is `true`.
// Returns `end(range1)` if no such iterator exists.
// Note: std::ranges::search(R1&& r1,...) returns a range, rather than an
// iterator. For simplicitly we match std::search's return type instead.
//
// Complexity: At most `size(range1) * size(range2)` applications of the
// corresponding predicate and projections.
//
// Reference: https://wg21.link/alg.search#:~:text=ranges::search(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = indirect_result_t<Pred&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>>
constexpr auto search(Range1&& range1,
                      Range2&& range2,
                      Pred pred = {},
                      Proj1 proj1 = {},
                      Proj2 proj2 = {}) {
  return ranges::search(ranges::begin(range1), ranges::end(range1),
                        ranges::begin(range2), ranges::end(range2),
                        std::move(pred), std::move(proj1), std::move(proj2));
}

// Mandates: The type `Size` is convertible to an integral type.
//
// Returns: `i` where `i` is the first iterator in the range
// `[first, last - count)` such that for every non-negative integer `n` less
// than `count`, the following condition holds:
// `invoke(pred, invoke(proj, *(i + n)), value)`.
// Returns `last` if no such iterator is found.
// Note: std::ranges::search_n(I1 first1,...) returns a range, rather than an
// iterator. For simplicitly we match std::search_n's return type instead.
//
// Complexity: At most `last - first` applications of the corresponding
// predicate and projection.
//
// Reference: https://wg21.link/alg.search#:~:text=ranges::search_n(I
template <typename ForwardIterator,
          typename Size,
          typename T,
          typename Pred = ranges::equal_to,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto search_n(ForwardIterator first,
                        ForwardIterator last,
                        Size count,
                        const T& value,
                        Pred pred = {},
                        Proj proj = {}) {
  // The second arg is guaranteed to be `value`, so we'll simply apply the
  // identity projection.
  identity value_proj;
  return std::search_n(
      first, last, count, value,
      internal::ProjectedBinaryPredicate(pred, proj, value_proj));
}

// Mandates: The type `Size` is convertible to an integral type.
//
// Returns: `i` where `i` is the first iterator in the range
// `[begin(range), end(range) - count)` such that for every non-negative integer
// `n` less than `count`, the following condition holds:
// `invoke(pred, invoke(proj, *(i + n)), value)`.
// Returns `end(arnge)` if no such iterator is found.
// Note: std::ranges::search_n(R1&& r1,...) returns a range, rather than an
// iterator. For simplicitly we match std::search_n's return type instead.
//
// Complexity: At most `size(range)` applications of the corresponding predicate
// and projection.
//
// Reference: https://wg21.link/alg.search#:~:text=ranges::search_n(R
template <typename Range,
          typename Size,
          typename T,
          typename Pred = ranges::equal_to,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto search_n(Range&& range,
                        Size count,
                        const T& value,
                        Pred pred = {},
                        Proj proj = {}) {
  return ranges::search_n(ranges::begin(range), ranges::end(range), count,
                          value, std::move(pred), std::move(proj));
}

// [alg.modifying.operations] Mutating sequence operations
// Reference: https://wg21.link/alg.modifying.operations

// [alg.copy] Copy
// Reference: https://wg21.link/alg.copy

// Let N be `last - first`.
//
// Preconditions: `result` is not in the range `[first, last)`.
//
// Effects: Copies elements in the range `[first, last)` into the range
// `[result, result + N)` starting from `first` and proceeding to `last`. For
// each non-negative integer `n < N` , performs `*(result + n) = *(first + n)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy(I
template <typename InputIterator,
          typename OutputIterator,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto copy(InputIterator first,
                    InputIterator last,
                    OutputIterator result) {
  return std::copy(first, last, result);
}

// Let N be `size(range)`.
//
// Preconditions: `result` is not in `range`.
//
// Effects: Copies elements in `range` into the range `[result, result + N)`
// starting from `begin(range)` and proceeding to `end(range)`. For each
// non-negative integer `n < N` , performs
// *(result + n) = *(begin(range) + n)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy(R
template <typename Range,
          typename OutputIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto copy(Range&& range, OutputIterator result) {
  return ranges::copy(ranges::begin(range), ranges::end(range), result);
}

// Let `N` be `max(0, n)`.
//
// Mandates: The type `Size` is convertible to an integral type.
//
// Effects: For each non-negative integer `i < N`, performs
// `*(result + i) = *(first + i)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy_n
template <typename InputIterator,
          typename Size,
          typename OutputIterator,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto copy_n(InputIterator first, Size n, OutputIterator result) {
  return std::copy_n(first, n, result);
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`, and `N` be the number
// of iterators `i` in the range `[first, last)` for which the condition `E(i)`
// holds.
//
// Preconditions: The ranges `[first, last)` and
// `[result, result + (last - first))` do not overlap.
//
// Effects: Copies all of the elements referred to by the iterator `i` in the
// range `[first, last)` for which `E(i)` is true.
//
// Returns: `result + N`
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy_if(I
template <typename InputIterator,
          typename OutputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto copy_if(InputIterator first,
                       InputIterator last,
                       OutputIterator result,
                       Pred pred,
                       Proj proj = {}) {
  return std::copy_if(first, last, result,
                      internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`, and `N` be the number
// of iterators `i` in `range` for which the condition `E(i)` holds.
//
// Preconditions: `range`  and `[result, result + size(range))` do not overlap.
//
// Effects: Copies all of the elements referred to by the iterator `i` in
// `range` for which `E(i)` is true.
//
// Returns: `result + N`
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy_if(R
template <typename Range,
          typename OutputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto copy_if(Range&& range,
                       OutputIterator result,
                       Pred pred,
                       Proj proj = {}) {
  return ranges::copy_if(ranges::begin(range), ranges::end(range), result,
                         std::move(pred), std::move(proj));
}

// Let `N` be `last - first`.
//
// Preconditions: `result` is not in the range `(first, last]`.
//
// Effects: Copies elements in the range `[first, last)` into the range
// `[result - N, result)` starting from `last - 1` and proceeding to `first`.
// For each positive integer `n ≤ N`, performs `*(result - n) = *(last - n)`.
//
// Returns: `result - N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy_backward(I1
template <typename BidirectionalIterator1,
          typename BidirectionalIterator2,
          typename = internal::iterator_category_t<BidirectionalIterator1>,
          typename = internal::iterator_category_t<BidirectionalIterator2>>
constexpr auto copy_backward(BidirectionalIterator1 first,
                             BidirectionalIterator1 last,
                             BidirectionalIterator2 result) {
  return std::copy_backward(first, last, result);
}

// Let `N` be `size(range)`.
//
// Preconditions: `result` is not in the range `(begin(range), end(range)]`.
//
// Effects: Copies elements in `range` into the range `[result - N, result)`
// starting from `end(range) - 1` and proceeding to `begin(range)`. For each
// positive integer `n ≤ N`, performs `*(result - n) = *(end(range) - n)`.
//
// Returns: `result - N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy_backward(R
template <typename Range,
          typename BidirectionalIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<BidirectionalIterator>>
constexpr auto copy_backward(Range&& range, BidirectionalIterator result) {
  return ranges::copy_backward(ranges::begin(range), ranges::end(range),
                               result);
}

// [alg.move] Move
// Reference: https://wg21.link/alg.move

// Let `E(n)` be `std::move(*(first + n))`.
//
// Let `N` be `last - first`.
//
// Preconditions: `result` is not in the range `[first, last)`.
//
// Effects: Moves elements in the range `[first, last)` into the range `[result,
// result + N)` starting from `first` and proceeding to `last`. For each
// non-negative integer `n < N`, performs `*(result + n) = E(n)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.move#:~:text=ranges::move(I
template <typename InputIterator,
          typename OutputIterator,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto move(InputIterator first,
                    InputIterator last,
                    OutputIterator result) {
  return std::move(first, last, result);
}

// Let `E(n)` be `std::move(*(begin(range) + n))`.
//
// Let `N` be `size(range)`.
//
// Preconditions: `result` is not in `range`.
//
// Effects: Moves elements in `range` into the range `[result, result + N)`
// starting from `begin(range)` and proceeding to `end(range)`. For each
// non-negative integer `n < N`, performs `*(result + n) = E(n)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.move#:~:text=ranges::move(R
template <typename Range,
          typename OutputIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto move(Range&& range, OutputIterator result) {
  return ranges::move(ranges::begin(range), ranges::end(range), result);
}

// Let `E(n)` be `std::move(*(last - n))`.
//
// Let `N` be `last - first`.
//
// Preconditions: `result` is not in the range `(first, last]`.
//
// Effects: Moves elements in the range `[first, last)` into the range
// `[result - N, result)` starting from `last - 1` and proceeding to `first`.
// For each positive integer `n ≤ N`, performs `*(result - n) = E(n)`.
//
// Returns: `result - N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.move#:~:text=ranges::move_backward(I1
template <typename BidirectionalIterator1,
          typename BidirectionalIterator2,
          typename = internal::iterator_category_t<BidirectionalIterator1>,
          typename = internal::iterator_category_t<BidirectionalIterator2>>
constexpr auto move_backward(BidirectionalIterator1 first,
                             BidirectionalIterator1 last,
                             BidirectionalIterator2 result) {
  return std::move_backward(first, last, result);
}

// Let `E(n)` be `std::move(*(end(range) - n))`.
//
// Let `N` be `size(range)`.
//
// Preconditions: `result` is not in the range `(begin(range), end(range)]`.
//
// Effects: Moves elements in `range` into the range `[result - N, result)`
// starting from `end(range) - 1` and proceeding to `begin(range)`. For each
// positive integer `n ≤ N`, performs `*(result - n) = E(n)`.
//
// Returns: `result - N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.move#:~:text=ranges::move_backward(R
template <typename Range,
          typename BidirectionalIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<BidirectionalIterator>>
constexpr auto move_backward(Range&& range, BidirectionalIterator result) {
  return ranges::move_backward(ranges::begin(range), ranges::end(range),
                               result);
}

// [alg.swap] Swap
// Reference: https://wg21.link/alg.swap

// Let `M` be `min(last1 - first1, last2 - first2)`.
//
// Preconditions: The two ranges `[first1, last1)` and `[first2, last2)` do not
// overlap. `*(first1 + n)` is swappable with `*(first2 + n)`.
//
// Effects: For each non-negative integer `n < M` performs
// `swap(*(first1 + n), *(first2 + n))`
//
// Returns: `first2 + M`
//
// Complexity: Exactly `M` swaps.
//
// Reference: https://wg21.link/alg.swap#:~:text=ranges::swap_ranges(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>>
constexpr auto swap_ranges(ForwardIterator1 first1,
                           ForwardIterator1 last1,
                           ForwardIterator2 first2,
                           ForwardIterator2 last2) {
  // std::swap_ranges does not have a `last2` overload. Thus we need to
  // adjust `last1` to ensure to not read past `last2`.
  last1 = std::next(first1, std::min(std::distance(first1, last1),
                                     std::distance(first2, last2)));
  return std::swap_ranges(first1, last1, first2);
}

// Let `M` be `min(size(range1), size(range2))`.
//
// Preconditions: The two ranges `range1` and `range2` do not overlap.
// `*(begin(range1) + n)` is swappable with `*(begin(range2) + n)`.
//
// Effects: For each non-negative integer `n < M` performs
// `swap(*(begin(range1) + n), *(begin(range2) + n))`
//
// Returns: `begin(range2) + M`
//
// Complexity: Exactly `M` swaps.
//
// Reference: https://wg21.link/alg.swap#:~:text=ranges::swap_ranges(R1
template <typename Range1,
          typename Range2,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>>
constexpr auto swap_ranges(Range1&& range1, Range2&& range2) {
  return ranges::swap_ranges(ranges::begin(range1), ranges::end(range1),
                             ranges::begin(range2), ranges::end(range2));
}

// [alg.transform] Transform
// Reference: https://wg21.link/alg.transform

// Let `N` be `last1 - first1`,
// `E(i)` be `invoke(op, invoke(proj, *(first1 + (i - result))))`.
//
// Preconditions: `op` does not invalidate iterators or subranges, nor modify
// elements in the ranges `[first1, first1 + N]`, and `[result, result + N]`.
//
// Effects: Assigns through every iterator `i` in the range
// `[result, result + N)` a new corresponding value equal to `E(i)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` applications of `op` and any projections.
//
// Remarks: result may be equal to `first1`.
//
// Reference: https://wg21.link/alg.transform#:~:text=ranges::transform(I
template <typename InputIterator,
          typename OutputIterator,
          typename UnaryOperation,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<UnaryOperation&,
                                       projected<InputIterator, Proj>>>
constexpr auto transform(InputIterator first1,
                         InputIterator last1,
                         OutputIterator result,
                         UnaryOperation op,
                         Proj proj = {}) {
  return std::transform(first1, last1, result, [&op, &proj](auto&& arg) {
    return base::invoke(op,
                        base::invoke(proj, std::forward<decltype(arg)>(arg)));
  });
}

// Let `N` be `size(range)`,
// `E(i)` be `invoke(op, invoke(proj, *(begin(range) + (i - result))))`.
//
// Preconditions: `op` does not invalidate iterators or subranges, nor modify
// elements in the ranges `[begin(range), end(range)]`, and
// `[result, result + N]`.
//
// Effects: Assigns through every iterator `i` in the range
// `[result, result + N)` a new corresponding value equal to `E(i)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` applications of `op` and any projections.
//
// Remarks: result may be equal to `begin(range)`.
//
// Reference: https://wg21.link/alg.transform#:~:text=ranges::transform(R
template <typename Range,
          typename OutputIterator,
          typename UnaryOperation,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<UnaryOperation&,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto transform(Range&& range,
                         OutputIterator result,
                         UnaryOperation op,
                         Proj proj = {}) {
  return ranges::transform(ranges::begin(range), ranges::end(range), result,
                           std::move(op), std::move(proj));
}

// Let:
// `N` be `min(last1 - first1, last2 - first2)`,
// `E(i)` be `invoke(binary_op, invoke(proj1, *(first1 + (i - result))),
//                              invoke(proj2, *(first2 + (i - result))))`.
//
// Preconditions: `binary_op` does not invalidate iterators or subranges, nor
// modify elements in the ranges `[first1, first1 + N]`, `[first2, first2 + N]`,
// and `[result, result + N]`.
//
// Effects: Assigns through every iterator `i` in the range
// `[result, result + N)` a new corresponding value equal to `E(i)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` applications of `binary_op`, and any projections.
//
// Remarks: `result` may be equal to `first1` or `first2`.
//
// Reference: https://wg21.link/alg.transform#:~:text=ranges::transform(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename OutputIterator,
          typename BinaryOperation,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<BinaryOperation&,
                                       projected<ForwardIterator1, Proj1>,
                                       projected<ForwardIterator2, Proj2>>>
constexpr auto transform(ForwardIterator1 first1,
                         ForwardIterator1 last1,
                         ForwardIterator2 first2,
                         ForwardIterator2 last2,
                         OutputIterator result,
                         BinaryOperation binary_op,
                         Proj1 proj1 = {},
                         Proj2 proj2 = {}) {
  // std::transform does not have a `last2` overload. Thus we need to adjust
  // `last1` to ensure to not read past `last2`.
  last1 = std::next(first1, std::min(std::distance(first1, last1),
                                     std::distance(first2, last2)));
  return std::transform(
      first1, last1, first2, result,
      [&binary_op, &proj1, &proj2](auto&& lhs, auto&& rhs) {
        return base::invoke(
            binary_op, base::invoke(proj1, std::forward<decltype(lhs)>(lhs)),
            base::invoke(proj2, std::forward<decltype(rhs)>(rhs)));
      });
}

// Let:
// `N` be `min(size(range1), size(range2)`,
// `E(i)` be `invoke(binary_op, invoke(proj1, *(begin(range1) + (i - result))),
//                              invoke(proj2, *(begin(range2) + (i - result))))`
//
// Preconditions: `binary_op` does not invalidate iterators or subranges, nor
// modify elements in the ranges `[begin(range1), end(range1)]`,
// `[begin(range2), end(range2)]`, and `[result, result + N]`.
//
// Effects: Assigns through every iterator `i` in the range
// `[result, result + N)` a new corresponding value equal to `E(i)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` applications of `binary_op`, and any projections.
//
// Remarks: `result` may be equal to `begin(range1)` or `begin(range2)`.
//
// Reference: https://wg21.link/alg.transform#:~:text=ranges::transform(R1
template <typename Range1,
          typename Range2,
          typename OutputIterator,
          typename BinaryOperation,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<BinaryOperation&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>>
constexpr auto transform(Range1&& range1,
                         Range2&& range2,
                         OutputIterator result,
                         BinaryOperation binary_op,
                         Proj1 proj1 = {},
                         Proj2 proj2 = {}) {
  return ranges::transform(ranges::begin(range1), ranges::end(range1),
                           ranges::begin(range2), ranges::end(range2), result,
                           std::move(binary_op), std::move(proj1),
                           std::move(proj2));
}

// [alg.replace] Replace
// Reference: https://wg21.link/alg.replace

// Let `E(i)` be `bool(invoke(proj, *i) == old_value)`.
//
// Mandates: `new_value` is writable  to `first`.
//
// Effects: Substitutes elements referred by the iterator `i` in the range
// `[first, last)` with `new_value`, when `E(i)` is true.
//
// Returns: `last`
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace(I
template <typename ForwardIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto replace(ForwardIterator first,
                       ForwardIterator last,
                       const T& old_value,
                       const T& new_value,
                       Proj proj = {}) {
  // Note: In order to be able to apply `proj` to each element in [first, last)
  // we are dispatching to std::replace_if instead of std::replace.
  std::replace_if(
      first, last,
      [&proj, &old_value](auto&& lhs) {
        return base::invoke(proj, std::forward<decltype(lhs)>(lhs)) ==
               old_value;
      },
      new_value);
  return last;
}

// Let `E(i)` be `bool(invoke(proj, *i) == old_value)`.
//
// Mandates: `new_value` is writable  to `begin(range)`.
//
// Effects: Substitutes elements referred by the iterator `i` in `range` with
// `new_value`, when `E(i)` is true.
//
// Returns: `end(range)`
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace(R
template <typename Range,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto replace(Range&& range,
                       const T& old_value,
                       const T& new_value,
                       Proj proj = {}) {
  return ranges::replace(ranges::begin(range), ranges::end(range), old_value,
                         new_value, std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Mandates: `new_value` is writable  to `first`.
//
// Effects: Substitutes elements referred by the iterator `i` in the range
// `[first, last)` with `new_value`, when `E(i)` is true.
//
// Returns: `last`
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_if(I
template <typename ForwardIterator,
          typename Predicate,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto replace_if(ForwardIterator first,
                          ForwardIterator last,
                          Predicate pred,
                          const T& new_value,
                          Proj proj = {}) {
  std::replace_if(first, last, internal::ProjectedUnaryPredicate(pred, proj),
                  new_value);
  return last;
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Mandates: `new_value` is writable  to `begin(range)`.
//
// Effects: Substitutes elements referred by the iterator `i` in `range` with
// `new_value`, when `E(i)` is true.
//
// Returns: `end(range)`
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_if(R
template <typename Range,
          typename Predicate,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto replace_if(Range&& range,
                          Predicate pred,
                          const T& new_value,
                          Proj proj = {}) {
  return ranges::replace_if(ranges::begin(range), ranges::end(range),
                            std::move(pred), new_value, std::move(proj));
}

// Let `E(i)` be `bool(invoke(proj, *(first + (i - result))) == old_value)`.
//
// Mandates: The results of the expressions `*first` and `new_value` are
// writable  to `result`.
//
// Preconditions: The ranges `[first, last)` and `[result, result + (last -
// first))` do not overlap.
//
// Effects: Assigns through every iterator `i` in the range `[result, result +
// (last - first))` a new corresponding value, `new_value` if `E(i)` is true, or
// `*(first + (i - result))` otherwise.
//
// Returns: `result + (last - first)`.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_copy(I
template <typename InputIterator,
          typename OutputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto replace_copy(InputIterator first,
                            InputIterator last,
                            OutputIterator result,
                            const T& old_value,
                            const T& new_value,
                            Proj proj = {}) {
  // Note: In order to be able to apply `proj` to each element in [first, last)
  // we are dispatching to std::replace_copy_if instead of std::replace_copy.
  std::replace_copy_if(
      first, last, result,
      [&proj, &old_value](auto&& lhs) {
        return base::invoke(proj, std::forward<decltype(lhs)>(lhs)) ==
               old_value;
      },
      new_value);
  return last;
}

// Let `E(i)` be
// `bool(invoke(proj, *(begin(range) + (i - result))) == old_value)`.
//
// Mandates: The results of the expressions `*begin(range)` and `new_value` are
// writable  to `result`.
//
// Preconditions: The ranges `range` and `[result, result + size(range))` do not
// overlap.
//
// Effects: Assigns through every iterator `i` in the range `[result, result +
// size(range))` a new corresponding value, `new_value` if `E(i)` is true, or
// `*(begin(range) + (i - result))` otherwise.
//
// Returns: `result + size(range)`.
//
// Complexity: Exactly `size(range)` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_copy(R
template <typename Range,
          typename OutputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto replace_copy(Range&& range,
                            OutputIterator result,
                            const T& old_value,
                            const T& new_value,
                            Proj proj = {}) {
  return ranges::replace_copy(ranges::begin(range), ranges::end(range), result,
                              old_value, new_value, std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *(first + (i - result)))))`.
//
// Mandates: The results of the expressions `*first` and `new_value` are
// writable  to `result`.
//
// Preconditions: The ranges `[first, last)` and `[result, result + (last -
// first))` do not overlap.
//
// Effects: Assigns through every iterator `i` in the range `[result, result +
// (last - first))` a new corresponding value, `new_value` if `E(i)` is true, or
// `*(first + (i - result))` otherwise.
//
// Returns: `result + (last - first)`.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_copy_if(I
template <typename InputIterator,
          typename OutputIterator,
          typename Predicate,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto replace_copy_if(InputIterator first,
                               InputIterator last,
                               OutputIterator result,
                               Predicate pred,
                               const T& new_value,
                               Proj proj = {}) {
  return std::replace_copy_if(first, last, result,
                              internal::ProjectedUnaryPredicate(pred, proj),
                              new_value);
}

// Let `E(i)` be
// `bool(invoke(pred, invoke(proj, *(begin(range) + (i - result)))))`.
//
// Mandates: The results of the expressions `*begin(range)` and `new_value` are
// writable  to `result`.
//
// Preconditions: The ranges `range` and `[result, result + size(range))` do not
// overlap.
//
// Effects: Assigns through every iterator `i` in the range `[result, result +
// size(range))` a new corresponding value, `new_value` if `E(i)` is true, or
// `*(begin(range) + (i - result))` otherwise.
//
// Returns: `result + size(range)`.
//
// Complexity: Exactly `size(range)` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_copy_if(R
template <typename Range,
          typename OutputIterator,
          typename Predicate,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto replace_copy_if(Range&& range,
                               OutputIterator result,
                               Predicate pred,
                               const T& new_value,
                               Proj proj = {}) {
  return ranges::replace_copy_if(ranges::begin(range), ranges::end(range),
                                 result, pred, new_value, std::move(proj));
}

// [alg.fill] Fill
// Reference: https://wg21.link/alg.fill

// Let `N` be `last - first`.
//
// Mandates: The expression `value` is writable to the output iterator.
//
// Effects: Assigns `value` through all the iterators in the range
// `[first, last)`.
//
// Returns: `last`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.fill#:~:text=ranges::fill(O
template <typename OutputIterator,
          typename T,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto fill(OutputIterator first, OutputIterator last, const T& value) {
  std::fill(first, last, value);
  return last;
}

// Let `N` be `size(range)`.
//
// Mandates: The expression `value` is writable to the output iterator.
//
// Effects: Assigns `value` through all the iterators in `range`.
//
// Returns: `end(range)`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.fill#:~:text=ranges::fill(R
template <typename Range,
          typename T,
          typename = internal::range_category_t<Range>>
constexpr auto fill(Range&& range, const T& value) {
  return ranges::fill(ranges::begin(range), ranges::end(range), value);
}

// Let `N` be `max(0, n)`.
//
// Mandates: The expression `value` is writable to the output iterator.
// The type `Size` is convertible to an integral type.
//
// Effects: Assigns `value` through all the iterators in `[first, first + N)`.
//
// Returns: `first + N`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.fill#:~:text=ranges::fill_n(O
template <typename OutputIterator,
          typename Size,
          typename T,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto fill_n(OutputIterator first, Size n, const T& value) {
  return std::fill_n(first, n, value);
}

// [alg.generate] Generate
// Reference: https://wg21.link/alg.generate

// Let `N` be `last - first`.
//
// Effects: Assigns the result of successive evaluations of gen() through each
// iterator in the range `[first, last)`.
//
// Returns: `last`.
//
// Complexity: Exactly `N` evaluations of `gen()` and assignments.
//
// Reference: https://wg21.link/alg.generate#:~:text=ranges::generate(O
template <typename OutputIterator,
          typename Generator,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto generate(OutputIterator first,
                        OutputIterator last,
                        Generator gen) {
  std::generate(first, last, std::move(gen));
  return last;
}

// Let `N` be `size(range)`.
//
// Effects: Assigns the result of successive evaluations of gen() through each
// iterator in `range`.
//
// Returns: `end(range)`.
//
// Complexity: Exactly `N` evaluations of `gen()` and assignments.
//
// Reference: https://wg21.link/alg.generate#:~:text=ranges::generate(R
template <typename Range,
          typename Generator,
          typename = internal::range_category_t<Range>>
constexpr auto generate(Range&& range, Generator gen) {
  return ranges::generate(ranges::begin(range), ranges::end(range),
                          std::move(gen));
}

// Let `N` be `max(0, n)`.
//
// Mandates: `Size` is convertible to an integral type.
//
// Effects: Assigns the result of successive evaluations of gen() through each
// iterator in the range `[first, first + N)`.
//
// Returns: `first + N`.
//
// Complexity: Exactly `N` evaluations of `gen()` and assignments.
//
// Reference: https://wg21.link/alg.generate#:~:text=ranges::generate_n(O
template <typename OutputIterator,
          typename Size,
          typename Generator,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto generate_n(OutputIterator first, Size n, Generator gen) {
  return std::generate_n(first, n, std::move(gen));
}

// [alg.remove] Remove
// Reference: https://wg21.link/alg.remove

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Effects: Eliminates all the elements referred to by iterator `i` in the range
// `[first, last)` for which `E(i)` holds.
//
// Returns: The end of the resulting range.
//
// Remarks: Stable.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove(I
template <typename ForwardIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto remove(ForwardIterator first,
                      ForwardIterator last,
                      const T& value,
                      Proj proj = {}) {
  // Note: In order to be able to apply `proj` to each element in [first, last)
  // we are dispatching to std::remove_if instead of std::remove.
  return std::remove_if(first, last, [&proj, &value](auto&& lhs) {
    return base::invoke(proj, std::forward<decltype(lhs)>(lhs)) == value;
  });
}

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Effects: Eliminates all the elements referred to by iterator `i` in `range`
// for which `E(i)` holds.
//
// Returns: The end of the resulting range.
//
// Remarks: Stable.
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove(R
template <typename Range,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto remove(Range&& range, const T& value, Proj proj = {}) {
  return ranges::remove(ranges::begin(range), ranges::end(range), value,
                        std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Effects: Eliminates all the elements referred to by iterator `i` in the range
// `[first, last)` for which `E(i)` holds.
//
// Returns: The end of the resulting range.
//
// Remarks: Stable.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_if(I
template <typename ForwardIterator,
          typename Predicate,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto remove_if(ForwardIterator first,
                         ForwardIterator last,
                         Predicate pred,
                         Proj proj = {}) {
  return std::remove_if(first, last,
                        internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Effects: Eliminates all the elements referred to by iterator `i` in `range`.
//
// Returns: The end of the resulting range.
//
// Remarks: Stable.
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_if(R
template <typename Range,
          typename Predicate,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto remove_if(Range&& range, Predicate pred, Proj proj = {}) {
  return ranges::remove_if(ranges::begin(range), ranges::end(range),
                           std::move(pred), std::move(proj));
}

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Let `N` be the number of elements in `[first, last)` for which `E(i)` is
// false.
//
// Mandates: `*first` is writable to `result`.
//
// Preconditions: The ranges `[first, last)` and `[result, result + (last -
// first))` do not overlap.
//
// Effects: Copies all the elements referred to by the iterator `i` in the range
// `[first, last)` for which `E(i)` is false.
//
// Returns: `result + N`.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_copy(I
template <typename InputIterator,
          typename OutputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto remove_copy(InputIterator first,
                           InputIterator last,
                           OutputIterator result,
                           const T& value,
                           Proj proj = {}) {
  // Note: In order to be able to apply `proj` to each element in [first, last)
  // we are dispatching to std::remove_copy_if instead of std::remove_copy.
  return std::remove_copy_if(first, last, result, [&proj, &value](auto&& lhs) {
    return base::invoke(proj, std::forward<decltype(lhs)>(lhs)) == value;
  });
}

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Let `N` be the number of elements in `range` for which `E(i)` is false.
//
// Mandates: `*begin(range)` is writable to `result`.
//
// Preconditions: The ranges `range` and `[result, result + size(range))` do not
// overlap.
//
// Effects: Copies all the elements referred to by the iterator `i` in `range`
//  for which `E(i)` is false.
//
// Returns: `result + N`.
//
// Complexity: Exactly `size(range)` applications of the corresponding
// predicate and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_copy(R
template <typename Range,
          typename OutputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto remove_copy(Range&& range,
                           OutputIterator result,
                           const T& value,
                           Proj proj = {}) {
  return ranges::remove_copy(ranges::begin(range), ranges::end(range), result,
                             value, std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Let `N` be the number of elements in `[first, last)` for which `E(i)` is
// false.
//
// Mandates: `*first` is writable to `result`.
//
// Preconditions: The ranges `[first, last)` and `[result, result + (last -
// first))` do not overlap.
//
// Effects: Copies all the elements referred to by the iterator `i` in the range
// `[first, last)` for which `E(i)` is false.
//
// Returns: `result + N`.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_copy_if(I
template <typename InputIterator,
          typename OutputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto remove_copy_if(InputIterator first,
                              InputIterator last,
                              OutputIterator result,
                              Pred pred,
                              Proj proj = {}) {
  return std::remove_copy_if(first, last, result,
                             internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Let `N` be the number of elements in `range` for which `E(i)` is false.
//
// Mandates: `*begin(range)` is writable to `result`.
//
// Preconditions: The ranges `range` and `[result, result + size(range))` do not
// overlap.
//
// Effects: Copies all the elements referred to by the iterator `i` in `range`
//  for which `E(i)` is false.
//
// Returns: `result + N`.
//
// Complexity: Exactly `size(range)` applications of the corresponding
// predicate and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_copy(R
template <typename Range,
          typename OutputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto remove_copy_if(Range&& range,
                              OutputIterator result,
                              Pred pred,
                              Proj proj = {}) {
  return ranges::remove_copy_if(ranges::begin(range), ranges::end(range),
                                result, std::move(pred), std::move(proj));
}

// [alg.unique] Unique
// Reference: https://wg21.link/alg.unique

// Let `E(i)` be `bool(invoke(comp, invoke(proj, *(i - 1)), invoke(proj, *i)))`.
//
// Effects: For a nonempty range, eliminates all but the first element from
// every consecutive group of equivalent elements referred to by the iterator
// `i` in the range `[first + 1, last)` for which `E(i)` is true.
//
// Returns: The end of the resulting range.
//
// Complexity: For nonempty ranges, exactly `(last - first) - 1` applications of
// the corresponding predicate and no more than twice as many applications of
// any projection.
//
// Reference: https://wg21.link/alg.unique#:~:text=ranges::unique(I
template <typename ForwardIterator,
          typename Comp = ranges::equal_to,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<ForwardIterator, Proj>,
                                       projected<ForwardIterator, Proj>>>
constexpr auto unique(ForwardIterator first,
                      ForwardIterator last,
                      Comp comp = {},
                      Proj proj = {}) {
  return std::unique(first, last,
                     internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Let `E(i)` be `bool(invoke(comp, invoke(proj, *(i - 1)), invoke(proj, *i)))`.
//
// Effects: For a nonempty range, eliminates all but the first element from
// every consecutive group of equivalent elements referred to by the iterator
// `i` in the range `[begin(range) + 1, end(range))` for which `E(i)` is true.
//
// Returns: The end of the resulting range.
//
// Complexity: For nonempty ranges, exactly `size(range) - 1` applications of
// the corresponding predicate and no more than twice as many applications of
// any projection.
//
// Reference: https://wg21.link/alg.unique#:~:text=ranges::unique(R
template <typename Range,
          typename Comp = ranges::equal_to,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto unique(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::unique(ranges::begin(range), ranges::end(range),
                        std::move(comp), std::move(proj));
}

// Let `E(i)` be `bool(invoke(comp, invoke(proj, *i), invoke(proj, *(i - 1))))`.
//
// Mandates: `*first` is writable to `result`.
//
// Preconditions: The ranges `[first, last)` and
// `[result, result + (last - first))` do not overlap.
//
// Effects: Copies only the first element from every consecutive group of equal
// elements referred to by the iterator `i` in the range `[first, last)` for
// which `E(i)` holds.
//
// Returns: `result + N`.
//
// Complexity: Exactly `last - first - 1` applications of the corresponding
// predicate and no more than twice as many applications of any projection.
//
// Reference: https://wg21.link/alg.unique#:~:text=ranges::unique_copy(I
template <typename ForwardIterator,
          typename OutputIterator,
          typename Comp = ranges::equal_to,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto unique_copy(ForwardIterator first,
                           ForwardIterator last,
                           OutputIterator result,
                           Comp comp = {},
                           Proj proj = {}) {
  return std::unique_copy(first, last, result,
                          internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Let `E(i)` be `bool(invoke(comp, invoke(proj, *i), invoke(proj, *(i - 1))))`.
//
// Mandates: `*begin(range)` is writable to `result`.
//
// Preconditions: The ranges `range` and `[result, result + size(range))` do not
// overlap.
//
// Effects: Copies only the first element from every consecutive group of equal
// elements referred to by the iterator `i` in `range` for which `E(i)` holds.
//
// Returns: `result + N`.
//
// Complexity: Exactly `size(range) - 1` applications of the corresponding
// predicate and no more than twice as many applications of any projection.
//
// Reference: https://wg21.link/alg.unique#:~:text=ranges::unique_copy(R
template <typename Range,
          typename OutputIterator,
          typename Comp = ranges::equal_to,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto unique_copy(Range&& range,
                           OutputIterator result,
                           Comp comp = {},
                           Proj proj = {}) {
  return ranges::unique_copy(ranges::begin(range), ranges::end(range), result,
                             std::move(comp), std::move(proj));
}

// [alg.reverse] Reverse
// Reference: https://wg21.link/alg.reverse

// Effects: For each non-negative integer `i < (last - first) / 2`, applies
// `std::iter_swap` to all pairs of iterators `first + i, (last - i) - 1`.
//
// Returns: `last`.
//
// Complexity: Exactly `(last - first)/2` swaps.
//
// Reference: https://wg21.link/alg.reverse#:~:text=ranges::reverse(I
template <typename BidirectionalIterator,
          typename = internal::iterator_category_t<BidirectionalIterator>>
constexpr auto reverse(BidirectionalIterator first,
                       BidirectionalIterator last) {
  std::reverse(first, last);
  return last;
}

// Effects: For each non-negative integer `i < size(range) / 2`, applies
// `std::iter_swap` to all pairs of iterators
// `begin(range) + i, (end(range) - i) - 1`.
//
// Returns: `end(range)`.
//
// Complexity: Exactly `size(range)/2` swaps.
//
// Reference: https://wg21.link/alg.reverse#:~:text=ranges::reverse(R
template <typename Range, typename = internal::range_category_t<Range>>
constexpr auto reverse(Range&& range) {
  return ranges::reverse(ranges::begin(range), ranges::end(range));
}

// Let `N` be `last - first`.
//
// Preconditions: The ranges `[first, last)` and `[result, result + N)` do not
// overlap.
//
// Effects: Copies the range `[first, last)` to the range `[result, result + N)`
// such that for every non-negative integer `i < N` the following assignment
// takes place: `*(result + N - 1 - i) = *(first + i)`.
//
// Returns: `result + N`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.reverse#:~:text=ranges::reverse_copy(I
template <typename BidirectionalIterator,
          typename OutputIterator,
          typename = internal::iterator_category_t<BidirectionalIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto reverse_copy(BidirectionalIterator first,
                            BidirectionalIterator last,
                            OutputIterator result) {
  return std::reverse_copy(first, last, result);
}

// Let `N` be `size(range)`.
//
// Preconditions: The ranges `range` and `[result, result + N)` do not
// overlap.
//
// Effects: Copies `range` to the range `[result, result + N)` such that for
// every non-negative integer `i < N` the following assignment takes place:
// `*(result + N - 1 - i) = *(begin(range) + i)`.
//
// Returns: `result + N`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.reverse#:~:text=ranges::reverse_copy(R
template <typename Range,
          typename OutputIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto reverse_copy(Range&& range, OutputIterator result) {
  return ranges::reverse_copy(ranges::begin(range), ranges::end(range), result);
}

// [alg.rotate] Rotate
// Reference: https://wg21.link/alg.rotate

// Preconditions: `[first, middle)` and `[middle, last)` are valid ranges.
//
// Effects: For each non-negative integer `i < (last - first)`, places the
// element from the position `first + i` into position
// `first + (i + (last - middle)) % (last - first)`.
//
// Returns: `first + (last - middle)`.
//
// Complexity: At most `last - first` swaps.
//
// Reference: https://wg21.link/alg.rotate#:~:text=ranges::rotate(I
template <typename ForwardIterator,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto rotate(ForwardIterator first,
                      ForwardIterator middle,
                      ForwardIterator last) {
  return std::rotate(first, middle, last);
}

// Preconditions: `[begin(range), middle)` and `[middle, end(range))` are valid
// ranges.
//
// Effects: For each non-negative integer `i < size(range)`, places the element
// from the position `begin(range) + i` into position
// `begin(range) + (i + (end(range) - middle)) % size(range)`.
//
// Returns: `begin(range) + (end(range) - middle)`.
//
// Complexity: At most `size(range)` swaps.
//
// Reference: https://wg21.link/alg.rotate#:~:text=ranges::rotate(R
template <typename Range, typename = internal::range_category_t<Range>>
constexpr auto rotate(Range&& range, iterator_t<Range> middle) {
  return ranges::rotate(ranges::begin(range), middle, ranges::end(range));
}

// Let `N` be `last - first`.
//
// Preconditions: `[first, middle)` and `[middle, last)` are valid ranges. The
// ranges `[first, last)` and `[result, result + N)` do not overlap.
//
// Effects: Copies the range `[first, last)` to the range `[result, result + N)`
// such that for each non-negative integer `i < N` the following assignment
// takes place: `*(result + i) = *(first + (i + (middle - first)) % N)`.
//
// Returns: `result + N`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.rotate#:~:text=ranges::rotate_copy(I
template <typename ForwardIterator,
          typename OutputIterator,
          typename = internal::iterator_category_t<ForwardIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto rotate_copy(ForwardIterator first,
                           ForwardIterator middle,
                           ForwardIterator last,
                           OutputIterator result) {
  return std::rotate_copy(first, middle, last, result);
}

// Let `N` be `size(range)`.
//
// Preconditions: `[begin(range), middle)` and `[middle, end(range))` are valid
// ranges. The ranges `range` and `[result, result + N)` do not overlap.
//
// Effects: Copies `range` to the range `[result, result + N)` such that for
// each non-negative integer `i < N` the following assignment takes place:
// `*(result + i) = *(begin(range) + (i + (middle - begin(range))) % N)`.
//
// Returns: `result + N`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.rotate#:~:text=ranges::rotate_copy(R
template <typename Range,
          typename OutputIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto rotate_copy(Range&& range,
                           iterator_t<Range> middle,
                           OutputIterator result) {
  return ranges::rotate_copy(ranges::begin(range), middle, ranges::end(range),
                             result);
}

// [alg.random.sample] Sample
// Reference: https://wg21.link/alg.random.sample

// Currently not implemented due to lack of std::sample in C++14.
// TODO(crbug.com/1071094): Consider implementing a hand-rolled version.

// [alg.random.shuffle] Shuffle
// Reference: https://wg21.link/alg.random.shuffle

// Preconditions: The type `std::remove_reference_t<UniformRandomBitGenerator>`
// meets the uniform random bit generator requirements.
//
// Effects: Permutes the elements in the range `[first, last)` such that each
// possible permutation of those elements has equal probability of appearance.
//
// Returns: `last`.
//
// Complexity: Exactly `(last - first) - 1` swaps.
//
// Remarks: To the extent that the implementation of this function makes use of
// random numbers, the object referenced by g shall serve as the
// implementation's source of randomness.
//
// Reference: https://wg21.link/alg.random.shuffle#:~:text=ranges::shuffle(I
template <typename RandomAccessIterator,
          typename UniformRandomBitGenerator,
          typename = internal::iterator_category_t<RandomAccessIterator>>
constexpr auto shuffle(RandomAccessIterator first,
                       RandomAccessIterator last,
                       UniformRandomBitGenerator&& g) {
  std::shuffle(first, last, std::forward<UniformRandomBitGenerator>(g));
  return last;
}

// Preconditions: The type `std::remove_reference_t<UniformRandomBitGenerator>`
// meets the uniform random bit generator requirements.
//
// Effects: Permutes the elements in `range` such that each possible permutation
// of those elements has equal probability of appearance.
//
// Returns: `end(range)`.
//
// Complexity: Exactly `size(range) - 1` swaps.
//
// Remarks: To the extent that the implementation of this function makes use of
// random numbers, the object referenced by g shall serve as the
// implementation's source of randomness.
//
// Reference: https://wg21.link/alg.random.shuffle#:~:text=ranges::shuffle(R
template <typename Range,
          typename UniformRandomBitGenerator,
          typename = internal::range_category_t<Range>>
constexpr auto shuffle(Range&& range, UniformRandomBitGenerator&& g) {
  return ranges::shuffle(ranges::begin(range), ranges::end(range),
                         std::forward<UniformRandomBitGenerator>(g));
}

// [alg.nonmodifying] Sorting and related operations
// Reference: https://wg21.link/alg.sorting

// [alg.sort] Sorting
// Reference: https://wg21.link/alg.sort

// [sort] sort
// Reference: https://wg21.link/sort

// Effects: Sorts the elements in the range `[first, last)` with respect to
// `comp` and `proj`.
//
// Returns: `last`.
//
// Complexity: Let `N` be `last - first`. `O(N log N)` comparisons and
// projections.
//
// Reference: https://wg21.link/sort#:~:text=ranges::sort(I
template <typename RandomAccessIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<RandomAccessIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<RandomAccessIterator, Proj>,
                                       projected<RandomAccessIterator, Proj>>>
constexpr auto sort(RandomAccessIterator first,
                    RandomAccessIterator last,
                    Comp comp = {},
                    Proj proj = {}) {
  std::sort(first, last, internal::ProjectedBinaryPredicate(comp, proj, proj));
  return last;
}

// Effects: Sorts the elements in `range` with respect to `comp` and `proj`.
//
// Returns: `end(range)`.
//
// Complexity: Let `N` be `size(range)`. `O(N log N)` comparisons and
// projections.
//
// Reference: https://wg21.link/sort#:~:text=ranges::sort(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto sort(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::sort(ranges::begin(range), ranges::end(range), std::move(comp),
                      std::move(proj));
}

// [stable.sort] stable_sort
// Reference: https://wg21.link/stable.sort

// Effects: Sorts the elements in the range `[first, last)` with respect to
// `comp` and `proj`.
//
// Returns: `last`.
//
// Complexity: Let `N` be `last - first`. If enough extra memory is available,
// `N log (N)` comparisons. Otherwise, at most `N log^2 (N)` comparisons. In
// either case, twice as many projections as the number of comparisons.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/stable.sort#:~:text=ranges::stable_sort(I
template <typename RandomAccessIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<RandomAccessIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<RandomAccessIterator, Proj>,
                                       projected<RandomAccessIterator, Proj>>>
constexpr auto stable_sort(RandomAccessIterator first,
                           RandomAccessIterator last,
                           Comp comp = {},
                           Proj proj = {}) {
  std::stable_sort(first, last,
                   internal::ProjectedBinaryPredicate(comp, proj, proj));
  return last;
}

// Effects: Sorts the elements in `range` with respect to `comp` and `proj`.
//
// Returns: `end(rang)`.
//
// Complexity: Let `N` be `size(range)`. If enough extra memory is available,
// `N log (N)` comparisons. Otherwise, at most `N log^2 (N)` comparisons. In
// either case, twice as many projections as the number of comparisons.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/stable.sort#:~:text=ranges::stable_sort(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto stable_sort(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::stable_sort(ranges::begin(range), ranges::end(range),
                             std::move(comp), std::move(proj));
}

// [partial.sort] partial_sort
// Reference: https://wg21.link/partial.sort

// Preconditions: `[first, middle)` and `[middle, last)` are valid ranges.
//
// Effects: Places the first `middle - first` elements from the range
// `[first, last)` as sorted with respect to `comp` and `proj` into the range
// `[first, middle)`. The rest of the elements in the range `[middle, last)` are
// placed in an unspecified order.
//
// Returns: `last`.
//
// Complexity: Approximately `(last - first) * log(middle - first)` comparisons,
// and twice as many projections.
//
// Reference: https://wg21.link/partial.sort#:~:text=ranges::partial_sort(I
template <typename RandomAccessIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<RandomAccessIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<RandomAccessIterator, Proj>,
                                       projected<RandomAccessIterator, Proj>>>
constexpr auto partial_sort(RandomAccessIterator first,
                            RandomAccessIterator middle,
                            RandomAccessIterator last,
                            Comp comp = {},
                            Proj proj = {}) {
  std::partial_sort(first, middle, last,
                    internal::ProjectedBinaryPredicate(comp, proj, proj));
  return last;
}

// Preconditions: `[begin(range), middle)` and `[middle, end(range))` are valid
// ranges.
//
// Effects: Places the first `middle - begin(range)` elements from `range` as
// sorted with respect to `comp` and `proj` into the range
// `[begin(range), middle)`. The rest of the elements in the range
// `[middle, end(range))` are placed in an unspecified order.
//
// Returns: `end(range)`.
//
// Complexity: Approximately `size(range) * log(middle - begin(range))`
// comparisons, and twice as many projections.
//
// Reference: https://wg21.link/partial.sort#:~:text=ranges::partial_sort(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto partial_sort(Range&& range,
                            iterator_t<Range> middle,
                            Comp comp = {},
                            Proj proj = {}) {
  return ranges::partial_sort(ranges::begin(range), middle, ranges::end(range),
                              std::move(comp), std::move(proj));
}

// [partial.sort.copy] partial_sort_copy
// Reference: https://wg21.link/partial.sort.copy

// Let `N` be `min(last - first, result_last - result_first)`.
//
// Preconditions: For iterators `a1` and `b1` in `[first, last)`, and iterators
// `x2` and `y2` in `[result_first, result_last)`, after evaluating the
// assignment `*y2 = *b1`, let `E` be the value of `bool(invoke(comp,
// invoke(proj1, *a1), invoke(proj2, *y2)))`. Then, after evaluating the
// assignment `*x2 = *a1`, `E` is equal to `bool(invoke(comp, invoke(proj2,
// *x2), invoke(proj2, *y2)))`.
//
// Effects: Places the first `N` elements as sorted with respect to `comp` and
// `proj2` into the range `[result_first, result_first + N)`.
//
// Returns: `result_first + N`.
//
// Complexity: Approximately `(last - first) * log N` comparisons, and twice as
// many projections.
//
// Reference:
// https://wg21.link/partial.sort.copy#:~:text=ranges::partial_sort_copy(I1
template <typename InputIterator,
          typename RandomAccessIterator,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<RandomAccessIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator, Proj1>,
                                       projected<RandomAccessIterator, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<RandomAccessIterator, Proj2>,
                                       projected<InputIterator, Proj1>>>
constexpr auto partial_sort_copy(InputIterator first,
                                 InputIterator last,
                                 RandomAccessIterator result_first,
                                 RandomAccessIterator result_last,
                                 Comp comp = {},
                                 Proj1 proj1 = {},
                                 Proj2 proj2 = {}) {
  // Needs to opt-in to all permutations, since std::partial_sort_copy expects
  // comp(proj2(lhs), proj1(rhs)) to compile.
  return std::partial_sort_copy(
      first, last, result_first, result_last,
      internal::PermutedProjectedBinaryPredicate(comp, proj1, proj2));
}

// Let `N` be `min(size(range), size(result_range))`.
//
// Preconditions: For iterators `a1` and `b1` in `range`, and iterators
// `x2` and `y2` in `result_range`, after evaluating the assignment
// `*y2 = *b1`, let `E` be the value of
// `bool(invoke(comp, invoke(proj1, *a1), invoke(proj2, *y2)))`. Then, after
// evaluating the assignment `*x2 = *a1`, `E` is equal to
// `bool(invoke(comp, invoke(proj2, *x2), invoke(proj2, *y2)))`.
//
// Effects: Places the first `N` elements as sorted with respect to `comp` and
// `proj2` into the range `[begin(result_range), begin(result_range) + N)`.
//
// Returns: `begin(result_range) + N`.
//
// Complexity: Approximately `size(range) * log N` comparisons, and twice as
// many projections.
//
// Reference:
// https://wg21.link/partial.sort.copy#:~:text=ranges::partial_sort_copy(R1
template <typename Range1,
          typename Range2,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range2>, Proj2>,
                                       projected<iterator_t<Range1>, Proj1>>>
constexpr auto partial_sort_copy(Range1&& range,
                                 Range2&& result_range,
                                 Comp comp = {},
                                 Proj1 proj1 = {},
                                 Proj2 proj2 = {}) {
  return ranges::partial_sort_copy(ranges::begin(range), ranges::end(range),
                                   ranges::begin(result_range),
                                   ranges::end(result_range), std::move(comp),
                                   std::move(proj1), std::move(proj2));
}

// [is.sorted] is_sorted
// Reference: https://wg21.link/is.sorted

// Returns: The last iterator `i` in `[first, last]` for which the range
// `[first, i)` is sorted with respect to `comp` and `proj`.
//
// Complexity: Linear.
//
// Reference: https://wg21.link/is.sorted#:~:text=ranges::is_sorted_until(I
template <typename ForwardIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<ForwardIterator, Proj>,
                                       projected<ForwardIterator, Proj>>>
constexpr auto is_sorted_until(ForwardIterator first,
                               ForwardIterator last,
                               Comp comp = {},
                               Proj proj = {}) {
  // Implementation inspired by cppreference.com:
  // https://en.cppreference.com/w/cpp/algorithm/is_sorted_until
  //
  // A reimplementation is required, because std::is_sorted_until is not
  // constexpr prior to C++20. Once we have C++20, we should switch to standard
  // library implementation.
  if (first == last)
    return last;

  for (ForwardIterator next = first; ++next != last; ++first) {
    if (base::invoke(comp, base::invoke(proj, *next),
                     base::invoke(proj, *first))) {
      return next;
    }
  }

  return last;
}

// Returns: The last iterator `i` in `[begin(range), end(range)]` for which the
// range `[begin(range), i)` is sorted with respect to `comp` and `proj`.
//
// Complexity: Linear.
//
// Reference: https://wg21.link/is.sorted#:~:text=ranges::is_sorted_until(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto is_sorted_until(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::is_sorted_until(ranges::begin(range), ranges::end(range),
                                 std::move(comp), std::move(proj));
}

// Returns: Whether the range `[first, last)` is sorted with respect to `comp`
// and `proj`.
//
// Complexity: Linear.
//
// Reference: https://wg21.link/is.sorted#:~:text=ranges::is_sorted(I
template <typename ForwardIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<ForwardIterator, Proj>,
                                       projected<ForwardIterator, Proj>>>
constexpr auto is_sorted(ForwardIterator first,
                         ForwardIterator last,
                         Comp comp = {},
                         Proj proj = {}) {
  return ranges::is_sorted_until(first, last, std::move(comp),
                                 std::move(proj)) == last;
}

// Returns: Whether `range` is sorted with respect to `comp` and `proj`.
//
// Complexity: Linear.
//
// Reference: https://wg21.link/is.sorted#:~:text=ranges::is_sorted(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto is_sorted(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::is_sorted(ranges::begin(range), ranges::end(range),
                           std::move(comp), std::move(proj));
}

// [alg.nth.element] Nth element
// Reference: https://wg21.link/alg.nth.element

// Preconditions: `[first, nth)` and `[nth, last)` are valid ranges.
//
// Effects: After `nth_element` the element in the position pointed to by `nth`
// is the element that would be in that position if the whole range were sorted
// with respect to `comp` and `proj`, unless `nth == last`. Also for every
// iterator `i` in the range `[first, nth)` and every iterator `j` in the range
// `[nth, last)` it holds that:
// `bool(invoke(comp, invoke(proj, *j), invoke(proj, *i)))` is false.
//
// Returns: `last`.
//
// Complexity: Linear on average.
//
// Reference: https://wg21.link/alg.nth.element#:~:text=ranges::nth_element(I
template <typename RandomAccessIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<RandomAccessIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<RandomAccessIterator, Proj>,
                                       projected<RandomAccessIterator, Proj>>>
constexpr auto nth_element(RandomAccessIterator first,
                           RandomAccessIterator nth,
                           RandomAccessIterator last,
                           Comp comp = {},
                           Proj proj = {}) {
  std::nth_element(first, nth, last,
                   internal::ProjectedBinaryPredicate(comp, proj, proj));
  return last;
}

// Preconditions: `[begin(range), nth)` and `[nth, end(range))` are valid
// ranges.
//
// Effects: After `nth_element` the element in the position pointed to by `nth`
// is the element that would be in that position if the whole range were sorted
// with respect to `comp` and `proj`, unless `nth == end(range)`. Also for every
// iterator `i` in the range `[begin(range), nth)` and every iterator `j` in the
// range `[nth, end(range))` it holds that:
// `bool(invoke(comp, invoke(proj, *j), invoke(proj, *i)))` is false.
//
// Returns: `end(range)`.
//
// Complexity: Linear on average.
//
// Reference: https://wg21.link/alg.nth.element#:~:text=ranges::nth_element(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto nth_element(Range&& range,
                           iterator_t<Range> nth,
                           Comp comp = {},
                           Proj proj = {}) {
  return ranges::nth_element(ranges::begin(range), nth, ranges::end(range),
                             std::move(comp), std::move(proj));
}

// [alg.binary.search] Binary search
// Reference: https://wg21.link/alg.binary.search

// [lower.bound] lower_bound
// Reference: https://wg21.link/lower.bound

// Preconditions: The elements `e` of `[first, last)` are partitioned with
// respect to the expression `bool(invoke(comp, invoke(proj, e), value))`.
//
// Returns: The furthermost iterator `i` in the range `[first, last]` such that
// for every iterator `j` in the range `[first, i)`,
// `bool(invoke(comp, invoke(proj, *j), value))` is true.
//
// Complexity: At most `log_2(last - first) + O(1)` comparisons and projections.
//
// Reference: https://wg21.link/lower.bound#:~:text=ranges::lower_bound(I
template <typename ForwardIterator,
          typename T,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto lower_bound(ForwardIterator first,
                           ForwardIterator last,
                           const T& value,
                           Comp comp = {},
                           Proj proj = {}) {
  // The second arg is guaranteed to be `value`, so we'll simply apply the
  // identity projection.
  identity value_proj;
  return std::lower_bound(
      first, last, value,
      internal::ProjectedBinaryPredicate(comp, proj, value_proj));
}

// Preconditions: The elements `e` of `range` are partitioned with respect to
// the expression `bool(invoke(comp, invoke(proj, e), value))`.
//
// Returns: The furthermost iterator `i` in the range
// `[begin(range), end(range)]` such that for every iterator `j` in the range
// `[begin(range), i)`, `bool(invoke(comp, invoke(proj, *j), value))` is true.
//
// Complexity: At most `log_2(size(range)) + O(1)` comparisons and projections.
//
// Reference: https://wg21.link/lower.bound#:~:text=ranges::lower_bound(R
template <typename Range,
          typename T,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto lower_bound(Range&& range,
                           const T& value,
                           Comp comp = {},
                           Proj proj = {}) {
  return ranges::lower_bound(ranges::begin(range), ranges::end(range), value,
                             std::move(comp), std::move(proj));
}

// [upper.bound] upper_bound
// Reference: https://wg21.link/upper.bound

// Preconditions: The elements `e` of `[first, last)` are partitioned with
// respect to the expression `!bool(invoke(comp, value, invoke(proj, e)))`.
//
// Returns: The furthermost iterator `i` in the range `[first, last]` such that
// for every iterator `j` in the range `[first, i)`,
// `!bool(invoke(comp, value, invoke(proj, *j)))` is true.
//
// Complexity: At most `log_2(last - first) + O(1)` comparisons and projections.
//
// Reference: https://wg21.link/upper.bound#:~:text=ranges::upper_bound(I
template <typename ForwardIterator,
          typename T,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto upper_bound(ForwardIterator first,
                           ForwardIterator last,
                           const T& value,
                           Comp comp = {},
                           Proj proj = {}) {
  // The first arg is guaranteed to be `value`, so we'll simply apply the
  // identity projection.
  identity value_proj;
  return std::upper_bound(
      first, last, value,
      internal::ProjectedBinaryPredicate(comp, value_proj, proj));
}

// Preconditions: The elements `e` of `range` are partitioned with
// respect to the expression `!bool(invoke(comp, value, invoke(proj, e)))`.
//
// Returns: The furthermost iterator `i` in the range
// `[begin(range), end(range)]` such that for every iterator `j` in the range
// `[begin(range), i)`, `!bool(invoke(comp, value, invoke(proj, *j)))` is true.
//
// Complexity: At most `log_2(size(range)) + O(1)` comparisons and projections.
//
// Reference: https://wg21.link/upper.bound#:~:text=ranges::upper_bound(R
template <typename Range,
          typename T,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto upper_bound(Range&& range,
                           const T& value,
                           Comp comp = {},
                           Proj proj = {}) {
  return ranges::upper_bound(ranges::begin(range), ranges::end(range), value,
                             std::move(comp), std::move(proj));
}

// [equal.range] equal_range
// Reference: https://wg21.link/equal.range

// Preconditions: The elements `e` of `[first, last)` are partitioned with
// respect to the expressions `bool(invoke(comp, invoke(proj, e), value))` and
// `!bool(invoke(comp, value, invoke(proj, e)))`.
//
// Returns: `{ranges::lower_bound(first, last, value, comp, proj),
//            ranges::upper_bound(first, last, value, comp, proj)}`.
//
// Complexity: At most 2 ∗ log_2(last - first) + O(1) comparisons and
// projections.
//
// Reference: https://wg21.link/equal.range#:~:text=ranges::equal_range(I
template <typename ForwardIterator,
          typename T,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto equal_range(ForwardIterator first,
                           ForwardIterator last,
                           const T& value,
                           Comp comp = {},
                           Proj proj = {}) {
  // Note: This does not dispatch to std::equal_range, as otherwise it would not
  // be possible to prevent applying `proj` to `value`, which can result in
  // unintended behavior.
  return std::make_pair(ranges::lower_bound(first, last, value, comp, proj),
                        ranges::upper_bound(first, last, value, comp, proj));
}

// Preconditions: The elements `e` of `range` are partitioned with
// respect to the expressions `bool(invoke(comp, invoke(proj, e), value))` and
// `!bool(invoke(comp, value, invoke(proj, e)))`.
//
// Returns: `{ranges::lower_bound(range, value, comp, proj),
//            ranges::upper_bound(range, value, comp, proj)}`.
//
// Complexity: At most 2 ∗ log_2(size(range)) + O(1) comparisons and
// projections.
//
// Reference: https://wg21.link/equal.range#:~:text=ranges::equal_range(R
template <typename Range,
          typename T,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto equal_range(Range&& range,
                           const T& value,
                           Comp comp = {},
                           Proj proj = {}) {
  return ranges::equal_range(ranges::begin(range), ranges::end(range), value,
                             std::move(comp), std::move(proj));
}

// [binary.search] binary_search
// Reference: https://wg21.link/binary.search

// Preconditions: The elements `e` of `[first, last)` are partitioned with
// respect to the expressions `bool(invoke(comp, invoke(proj, e), value))` and
// `!bool(invoke(comp, value, invoke(proj, e)))`.
//
// Returns: `true` if and only if for some iterator `i` in the range
// `[first, last)`, `!bool(invoke(comp, invoke(proj, *i), value)) &&
//                   !bool(invoke(comp, value, invoke(proj, *i)))` is true.
//
// Complexity: At most `log_2(last - first) + O(1)` comparisons and projections.
//
// Reference: https://wg21.link/binary.search#:~:text=ranges::binary_search(I
template <typename ForwardIterator,
          typename T,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto binary_search(ForwardIterator first,
                             ForwardIterator last,
                             const T& value,
                             Comp comp = {},
                             Proj proj = {}) {
  first = ranges::lower_bound(first, last, value, comp, proj);
  return first != last &&
         !base::invoke(comp, value, base::invoke(proj, *first));
}

// Preconditions: The elements `e` of `range` are partitioned with
// respect to the expressions `bool(invoke(comp, invoke(proj, e), value))` and
// `!bool(invoke(comp, value, invoke(proj, e)))`.
//
// Returns: `true` if and only if for some iterator `i` in `range`
// `!bool(invoke(comp, invoke(proj, *i), value)) &&
//  !bool(invoke(comp, value, invoke(proj, *i)))` is true.
//
// Complexity: At most `log_2(size(range)) + O(1)` comparisons and projections.
//
// Reference: https://wg21.link/binary.search#:~:text=ranges::binary_search(R
template <typename Range,
          typename T,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto binary_search(Range&& range,
                             const T& value,
                             Comp comp = {},
                             Proj proj = {}) {
  return ranges::binary_search(ranges::begin(range), ranges::end(range), value,
                               std::move(comp), std::move(proj));
}

// [alg.partitions] Partitions
// Reference: https://wg21.link/alg.partitions

// Returns: `true` if and only if the elements `e` of `[first, last)` are
// partitioned with respect to the expression
// `bool(invoke(pred, invoke(proj, e)))`.
//
// Complexity: Linear. At most `last - first` applications of `pred` and `proj`.
//
// Reference: https://wg21.link/alg.partitions#:~:text=ranges::is_partitioned(I
template <typename ForwardIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto is_partitioned(ForwardIterator first,
                              ForwardIterator last,
                              Pred pred,
                              Proj proj = {}) {
  return std::is_partitioned(first, last,
                             internal::ProjectedUnaryPredicate(pred, proj));
}

// Returns: `true` if and only if the elements `e` of `range` are partitioned
// with respect to the expression `bool(invoke(pred, invoke(proj, e)))`.
//
// Complexity: Linear. At most `size(range)` applications of `pred` and `proj`.
//
// Reference: https://wg21.link/alg.partitions#:~:text=ranges::is_partitioned(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto is_partitioned(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::is_partitioned(ranges::begin(range), ranges::end(range),
                                std::move(pred), std::move(proj));
}

// Let `E(x)` be `bool(invoke(pred, invoke(proj, x)))`.
//
// Effects: Places all the elements `e` in `[first, last)` that satisfy `E(e)`
// before all the elements that do not.
//
// Returns: Let `i` be an iterator such that `E(*j)` is `true` for every
// iterator `j` in `[first, i)` and `false` for every iterator `j` in
// `[i, last)`. Returns: i.
//
// Complexity: Let `N = last - first`:
// Exactly `N` applications of the predicate and projection. At most `N / 2`
// swaps if the type of `first` models `bidirectional_iterator`, and at most `N`
// swaps otherwise.
//
// Reference: https://wg21.link/alg.partitions#:~:text=ranges::partition(I
template <typename ForwardIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto partition(ForwardIterator first,
                         ForwardIterator last,
                         Pred pred,
                         Proj proj = {}) {
  return std::partition(first, last,
                        internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(x)` be `bool(invoke(pred, invoke(proj, x)))`.
//
// Effects: Places all the elements `e` in `range` that satisfy `E(e)` before
// all the elements that do not.
//
// Returns: Let `i` be an iterator such that `E(*j)` is `true` for every
// iterator `j` in `[begin(range), i)` and `false` for every iterator `j` in
// `[i, last)`. Returns: i.
//
// Complexity: Let `N = size(range)`:
// Exactly `N` applications of the predicate and projection. At most `N / 2`
// swaps if the type of `first` models `bidirectional_iterator`, and at most `N`
// swaps otherwise.
//
// Reference: https://wg21.link/alg.partitions#:~:text=ranges::partition(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto partition(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::partition(ranges::begin(range), ranges::end(range),
                           std::move(pred), std::move(proj));
}

// Let `E(x)` be `bool(invoke(pred, invoke(proj, x)))`.
//
// Effects: Places all the elements `e` in `[first, last)` that satisfy `E(e)`
// before all the elements that do not. The relative order of the elements in
// both groups is preserved.
//
// Returns: Let `i` be an iterator such that for every iterator `j` in
// `[first, i)`, `E(*j)` is `true`, and for every iterator `j` in the range
// `[i, last)`, `E(*j)` is `false`. Returns: `i`.
//
// Complexity: Let `N = last - first`:
// At most `N log N` swaps, but only `O(N)` swaps if there is enough extra
// memory. Exactly `N` applications of the predicate and projection.
//
// Reference:
// https://wg21.link/alg.partitions#:~:text=ranges::stable_partition(I
template <typename BidirectionalIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<BidirectionalIterator>>
constexpr auto stable_partition(BidirectionalIterator first,
                                BidirectionalIterator last,
                                Pred pred,
                                Proj proj = {}) {
  return std::stable_partition(first, last,
                               internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(x)` be `bool(invoke(pred, invoke(proj, x)))`.
//
// Effects: Places all the elements `e` in `range` that satisfy `E(e)` before
// all the elements that do not. The relative order of the elements in both
// groups is preserved.
//
// Returns: Let `i` be an iterator such that for every iterator `j` in
// `[begin(range), i)`, `E(*j)` is `true`, and for every iterator `j` in the
// range `[i, end(range))`, `E(*j)` is `false`. Returns: `i`.
//
// Complexity: Let `N = size(range)`:
// At most `N log N` swaps, but only `O(N)` swaps if there is enough extra
// memory. Exactly `N` applications of the predicate and projection.
//
// Reference:
// https://wg21.link/alg.partitions#:~:text=ranges::stable_partition(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto stable_partition(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::stable_partition(ranges::begin(range), ranges::end(range),
                                  std::move(pred), std::move(proj));
}

// Let `E(x)` be `bool(invoke(pred, invoke(proj, x)))`.
//
// Mandates: The expression `*first` is writable to `out_true` and `out_false`.
//
// Preconditions: The input range and output ranges do not overlap.
//
// Effects: For each iterator `i` in `[first, last)`, copies `*i` to the output
// range beginning with `out_true` if `E(*i)` is `true`, or to the output range
// beginning with `out_false` otherwise.
//
// Returns: Let `o1` be the end of the output range beginning at `out_true`, and
// `o2` the end of the output range beginning at `out_false`.
// Returns `{o1, o2}`.
//
// Complexity: Exactly `last - first` applications of `pred` and `proj`.
//
// Reference: https://wg21.link/alg.partitions#:~:text=ranges::partition_copy(I
template <typename InputIterator,
          typename OutputIterator1,
          typename OutputIterator2,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator1>,
          typename = internal::iterator_category_t<OutputIterator2>>
constexpr auto partition_copy(InputIterator first,
                              InputIterator last,
                              OutputIterator1 out_true,
                              OutputIterator2 out_false,
                              Pred pred,
                              Proj proj = {}) {
  return std::partition_copy(first, last, out_true, out_false,
                             internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(x)` be `bool(invoke(pred, invoke(proj, x)))`.
//
// Mandates: The expression `*begin(range)` is writable to `out_true` and
// `out_false`.
//
// Preconditions: The input range and output ranges do not overlap.
//
// Effects: For each iterator `i` in `range`, copies `*i` to the output range
// beginning with `out_true` if `E(*i)` is `true`, or to the output range
// beginning with `out_false` otherwise.
//
// Returns: Let `o1` be the end of the output range beginning at `out_true`, and
// `o2` the end of the output range beginning at `out_false`.
// Returns `{o1, o2}`.
//
// Complexity: Exactly `size(range)` applications of `pred` and `proj`.
//
// Reference: https://wg21.link/alg.partitions#:~:text=ranges::partition_copy(R
template <typename Range,
          typename OutputIterator1,
          typename OutputIterator2,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator1>,
          typename = internal::iterator_category_t<OutputIterator2>>
constexpr auto partition_copy(Range&& range,
                              OutputIterator1 out_true,
                              OutputIterator2 out_false,
                              Pred pred,
                              Proj proj = {}) {
  return ranges::partition_copy(ranges::begin(range), ranges::end(range),
                                out_true, out_false, std::move(pred),
                                std::move(proj));
}

// let `E(x)` be `bool(invoke(pred, invoke(proj, x)))`.
//
// Preconditions: The elements `e` of `[first, last)` are partitioned with
// respect to `E(e)`.
//
// Returns: An iterator `mid` such that `E(*i)` is `true` for all iterators `i`
// in `[first, mid)`, and `false` for all iterators `i` in `[mid, last)`.
//
// Complexity: `O(log(last - first))` applications of `pred` and `proj`.
//
// Reference: https://wg21.link/alg.partitions#:~:text=ranges::partition_point(I
template <typename ForwardIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto partition_point(ForwardIterator first,
                               ForwardIterator last,
                               Pred pred,
                               Proj proj = {}) {
  return std::partition_point(first, last,
                              internal::ProjectedUnaryPredicate(pred, proj));
}

// let `E(x)` be `bool(invoke(pred, invoke(proj, x)))`.
//
// Preconditions: The elements `e` of `range` are partitioned with respect to
// `E(e)`.
//
// Returns: An iterator `mid` such that `E(*i)` is `true` for all iterators `i`
// in `[begin(range), mid)`, and `false` for all iterators `i` in
// `[mid, end(range))`.
//
// Complexity: `O(log(size(range)))` applications of `pred` and `proj`.
//
// Reference: https://wg21.link/alg.partitions#:~:text=ranges::partition_point(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto partition_point(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::partition_point(ranges::begin(range), ranges::end(range),
                                 std::move(pred), std::move(proj));
}

// [alg.merge] Merge
// Reference: https://wg21.link/alg.merge

// Let `N` be `(last1 - first1) + (last2 - first2)`.
//
// Preconditions: The ranges `[first1, last1)` and `[first2, last2)` are sorted
// with respect to `comp` and `proj1` or `proj2`, respectively. The resulting
// range does not overlap with either of the original ranges.
//
// Effects: Copies all the elements of the two ranges `[first1, last1)` and
// `[first2, last2)` into the range `[result, result_last)`, where `result_last`
// is `result + N`. If an element `a` precedes `b` in an input range, `a` is
// copied into the output range before `b`. If `e1` is an element of
// `[first1, last1)` and `e2` of `[first2, last2)`, `e2` is copied into the
// output range before `e1` if and only if
// `bool(invoke(comp, invoke(proj2, e2), invoke(proj1, e1)))` is `true`.
//
// Returns: `result_last`.
//
// Complexity: At most `N - 1` comparisons and applications of each projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.merge#:~:text=ranges::merge(I1
template <typename InputIterator1,
          typename InputIterator2,
          typename OutputIterator,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<InputIterator1>,
          typename = internal::iterator_category_t<InputIterator2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator1, Proj1>,
                                       projected<InputIterator2, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator2, Proj2>,
                                       projected<InputIterator1, Proj1>>>
constexpr auto merge(InputIterator1 first1,
                     InputIterator1 last1,
                     InputIterator2 first2,
                     InputIterator2 last2,
                     OutputIterator result,
                     Comp comp = {},
                     Proj1 proj1 = {},
                     Proj2 proj2 = {}) {
  // Needs to opt-in to all permutations, since std::merge expects
  // comp(proj2(lhs), proj1(rhs)) to compile.
  return std::merge(
      first1, last1, first2, last2, result,
      internal::PermutedProjectedBinaryPredicate(comp, proj1, proj2));
}

// Let `N` be `size(range1) + size(range2)`.
//
// Preconditions: The ranges `range1` and `range2` are sorted with respect to
// `comp` and `proj1` or `proj2`, respectively. The resulting range does not
// overlap with either of the original ranges.
//
// Effects: Copies all the elements of the two ranges `range1` and `range2` into
// the range `[result, result_last)`, where `result_last` is `result + N`. If an
// element `a` precedes `b` in an input range, `a` is copied into the output
// range before `b`. If `e1` is an element of `range1` and `e2` of `range2`,
// `e2` is copied into the output range before `e1` if and only if
// `bool(invoke(comp, invoke(proj2, e2), invoke(proj1, e1)))` is `true`.
//
// Returns: `result_last`.
//
// Complexity: At most `N - 1` comparisons and applications of each projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.merge#:~:text=ranges::merge(R1
template <typename Range1,
          typename Range2,
          typename OutputIterator,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range2>, Proj2>,
                                       projected<iterator_t<Range1>, Proj1>>>
constexpr auto merge(Range1&& range1,
                     Range2&& range2,
                     OutputIterator result,
                     Comp comp = {},
                     Proj1 proj1 = {},
                     Proj2 proj2 = {}) {
  return ranges::merge(ranges::begin(range1), ranges::end(range1),
                       ranges::begin(range2), ranges::end(range2), result,
                       std::move(comp), std::move(proj1), std::move(proj2));
}

// Preconditions: `[first, middle)` and `[middle, last)` are valid ranges sorted
// with respect to `comp` and `proj`.
//
// Effects: Merges two sorted consecutive ranges `[first, middle)` and
// `[middle, last)`, putting the result of the merge into the range
// `[first, last)`. The resulting range is sorted with respect to `comp` and
// `proj`.
//
// Returns: `last`.
//
// Complexity: Let `N = last - first`: If enough additional memory is available,
// exactly `N - 1` comparisons. Otherwise, `O(N log N)` comparisons. In either
// case, twice as many projections as comparisons.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.merge#:~:text=ranges::inplace_merge(I
template <typename BidirectionalIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<BidirectionalIterator>>
constexpr auto inplace_merge(BidirectionalIterator first,
                             BidirectionalIterator middle,
                             BidirectionalIterator last,
                             Comp comp = {},
                             Proj proj = {}) {
  std::inplace_merge(first, middle, last,
                     internal::ProjectedBinaryPredicate(comp, proj, proj));
  return last;
}

// Preconditions: `[begin(range), middle)` and `[middle, end(range))` are valid
// ranges sorted with respect to `comp` and `proj`.
//
// Effects: Merges two sorted consecutive ranges `[begin(range), middle)` and
// `[middle, end(range))`, putting the result of the merge into `range`. The
// resulting range is sorted with respect to `comp` and `proj`.
//
// Returns: `end(range)`.
//
// Complexity: Let `N = size(range)`: If enough additional memory is available,
// exactly `N - 1` comparisons. Otherwise, `O(N log N)` comparisons. In either
// case, twice as many projections as comparisons.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.merge#:~:text=ranges::inplace_merge(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto inplace_merge(Range&& range,
                             iterator_t<Range> middle,
                             Comp comp = {},
                             Proj proj = {}) {
  return ranges::inplace_merge(ranges::begin(range), middle, ranges::end(range),
                               std::move(comp), std::move(proj));
}

// [alg.set.operations] Set operations on sorted structures
// Reference: https://wg21.link/alg.set.operations

// [includes] includes
// Reference: https://wg21.link/includes

// Preconditions: The ranges `[first1, last1)` and `[first2, last2)` are sorted
// with respect to `comp` and `proj1` or `proj2`, respectively.
//
// Returns: `true` if and only if `[first2, last2)` is a subsequence of
// `[first1, last1)`.
//
// Complexity: At most `2 * ((last1 - first1) + (last2 - first2)) - 1`
// comparisons and applications of each projection.
//
// Reference: https://wg21.link/includes#:~:text=ranges::includes(I1
template <typename InputIterator1,
          typename InputIterator2,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<InputIterator1>,
          typename = internal::iterator_category_t<InputIterator2>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator1, Proj1>,
                                       projected<InputIterator2, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator2, Proj2>,
                                       projected<InputIterator1, Proj1>>>
constexpr auto includes(InputIterator1 first1,
                        InputIterator1 last1,
                        InputIterator2 first2,
                        InputIterator2 last2,
                        Comp comp = {},
                        Proj1 proj1 = {},
                        Proj2 proj2 = {}) {
  DCHECK(ranges::is_sorted(first1, last1, comp, proj1));
  DCHECK(ranges::is_sorted(first2, last2, comp, proj2));
  // Needs to opt-in to all permutations, since std::includes expects
  // comp(proj1(lhs), proj2(rhs)) and comp(proj2(lhs), proj1(rhs)) to compile.
  return std::includes(
      first1, last1, first2, last2,
      internal::PermutedProjectedBinaryPredicate(comp, proj1, proj2));
}

// Preconditions: The ranges `range1` and `range2` are sorted with respect to
// `comp` and `proj1` or `proj2`, respectively.
//
// Returns: `true` if and only if `range2` is a subsequence of `range1`.
//
// Complexity: At most `2 * (size(range1) + size(range2)) - 1` comparisons and
// applications of each projection.
//
// Reference: https://wg21.link/includes#:~:text=ranges::includes(R1
template <typename Range1,
          typename Range2,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range2>, Proj2>,
                                       projected<iterator_t<Range1>, Proj1>>>
constexpr auto includes(Range1&& range1,
                        Range2&& range2,
                        Comp comp = {},
                        Proj1 proj1 = {},
                        Proj2 proj2 = {}) {
  return ranges::includes(ranges::begin(range1), ranges::end(range1),
                          ranges::begin(range2), ranges::end(range2),
                          std::move(comp), std::move(proj1), std::move(proj2));
}

// [set.union] set_union
// Reference: https://wg21.link/set.union

// Preconditions: The ranges `[first1, last1)` and `[first2, last2)` are sorted
// with respect to `comp` and `proj1` or `proj2`, respectively. The resulting
// range does not overlap with either of the original ranges.
//
// Effects: Constructs a sorted union of the elements from the two ranges; that
// is, the set of elements that are present in one or both of the ranges.
//
// Returns: The end of the constructed range.
//
// Complexity: At most `2 * ((last1 - first1) + (last2 - first2)) - 1`
// comparisons and applications of each projection.
//
// Remarks: Stable. If `[first1, last1)` contains `m` elements that are
// equivalent to each other and `[first2, last2)` contains `n` elements that are
// equivalent to them, then all `m` elements from the first range are copied to
// the output range, in order, and then the final `max(n - m , 0)` elements from
// the second range are copied to the output range, in order.
//
// Reference: https://wg21.link/set.union#:~:text=ranges::set_union(I1
template <typename InputIterator1,
          typename InputIterator2,
          typename OutputIterator,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<InputIterator1>,
          typename = internal::iterator_category_t<InputIterator2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator1, Proj1>,
                                       projected<InputIterator2, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator2, Proj2>,
                                       projected<InputIterator1, Proj1>>>
constexpr auto set_union(InputIterator1 first1,
                         InputIterator1 last1,
                         InputIterator2 first2,
                         InputIterator2 last2,
                         OutputIterator result,
                         Comp comp = {},
                         Proj1 proj1 = {},
                         Proj2 proj2 = {}) {
  // Needs to opt-in to all permutations, since std::set_union expects
  // comp(proj1(lhs), proj2(rhs)) and comp(proj2(lhs), proj1(rhs)) to compile.
  return std::set_union(
      first1, last1, first2, last2, result,
      internal::PermutedProjectedBinaryPredicate(comp, proj1, proj2));
}

// Preconditions: The ranges `range1` and `range2` are sorted with respect to
// `comp` and `proj1` or `proj2`, respectively. The resulting range does not
// overlap with either of the original ranges.
//
// Effects: Constructs a sorted union of the elements from the two ranges; that
// is, the set of elements that are present in one or both of the ranges.
//
// Returns: The end of the constructed range.
//
// Complexity: At most `2 * (size(range1) + size(range2)) - 1` comparisons and
// applications of each projection.
//
// Remarks: Stable. If `range1` contains `m` elements that are equivalent to
// each other and `range2` contains `n` elements that are equivalent to them,
// then all `m` elements from the first range are copied to the output range, in
// order, and then the final `max(n - m , 0)` elements from the second range are
// copied to the output range, in order.
//
// Reference: https://wg21.link/set.union#:~:text=ranges::set_union(R1
template <typename Range1,
          typename Range2,
          typename OutputIterator,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range2>, Proj2>,
                                       projected<iterator_t<Range1>, Proj1>>>
constexpr auto set_union(Range1&& range1,
                         Range2&& range2,
                         OutputIterator result,
                         Comp comp = {},
                         Proj1 proj1 = {},
                         Proj2 proj2 = {}) {
  return ranges::set_union(ranges::begin(range1), ranges::end(range1),
                           ranges::begin(range2), ranges::end(range2), result,
                           std::move(comp), std::move(proj1), std::move(proj2));
}

// [set.intersection] set_intersection
// Reference: https://wg21.link/set.intersection

// Preconditions: The ranges `[first1, last1)` and `[first2, last2)` are sorted
// with respect to `comp` and `proj1` or `proj2`, respectively. The resulting
// range does not overlap with either of the original ranges.
//
// Effects: Constructs a sorted intersection of the elements from the two
// ranges; that is, the set of elements that are present in both of the ranges.
//
// Returns: The end of the constructed range.
//
// Complexity: At most `2 * ((last1 - first1) + (last2 - first2)) - 1`
// comparisons and applications of each projection.
//
// Remarks: Stable. If `[first1, last1)` contains `m` elements that are
// equivalent to each other and `[first2, last2)` contains `n` elements that are
// equivalent to them, the first `min(m, n)` elements are copied from the first
// range to the output range, in order.
//
// Reference:
// https://wg21.link/set.intersection#:~:text=ranges::set_intersection(I1
template <typename InputIterator1,
          typename InputIterator2,
          typename OutputIterator,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<InputIterator1>,
          typename = internal::iterator_category_t<InputIterator2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator1, Proj1>,
                                       projected<InputIterator2, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator2, Proj2>,
                                       projected<InputIterator1, Proj1>>>
constexpr auto set_intersection(InputIterator1 first1,
                                InputIterator1 last1,
                                InputIterator2 first2,
                                InputIterator2 last2,
                                OutputIterator result,
                                Comp comp = {},
                                Proj1 proj1 = {},
                                Proj2 proj2 = {}) {
  // Needs to opt-in to all permutations, since std::set_intersection expects
  // comp(proj1(lhs), proj2(rhs)) and comp(proj2(lhs), proj1(rhs)) to compile.
  return std::set_intersection(
      first1, last1, first2, last2, result,
      internal::PermutedProjectedBinaryPredicate(comp, proj1, proj2));
}

// Preconditions: The ranges `range1` and `range2` are sorted with respect to
// `comp` and `proj1` or `proj2`, respectively. The resulting range does not
// overlap with either of the original ranges.
//
// Effects: Constructs a sorted intersection of the elements from the two
// ranges; that is, the set of elements that are present in both of the ranges.
//
// Returns: The end of the constructed range.
//
// Complexity: At most `2 * (size(range1) + size(range2)) - 1` comparisons and
// applications of each projection.
//
// Remarks: Stable. If `range1` contains `m` elements that are equivalent to
// each other and `range2` contains `n` elements that are equivalent to them,
// the first `min(m, n)` elements are copied from the first range to the output
// range, in order.
//
// Reference:
// https://wg21.link/set.intersection#:~:text=ranges::set_intersection(R1
template <typename Range1,
          typename Range2,
          typename OutputIterator,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range2>, Proj2>,
                                       projected<iterator_t<Range1>, Proj1>>>
constexpr auto set_intersection(Range1&& range1,
                                Range2&& range2,
                                OutputIterator result,
                                Comp comp = {},
                                Proj1 proj1 = {},
                                Proj2 proj2 = {}) {
  return ranges::set_intersection(ranges::begin(range1), ranges::end(range1),
                                  ranges::begin(range2), ranges::end(range2),
                                  result, std::move(comp), std::move(proj1),
                                  std::move(proj2));
}

// [set.difference] set_difference
// Reference: https://wg21.link/set.difference

// Preconditions: The ranges `[first1, last1)` and `[first2, last2)` are sorted
// with respect to `comp` and `proj1` or `proj2`, respectively. The resulting
// range does not overlap with either of the original ranges.
//
// Effects: Copies the elements of the range `[first1, last1)` which are not
// present in the range `[first2, last2)` to the range beginning at `result`.
// The elements in the constructed range are sorted.
//
// Returns: The end of the constructed range.
//
// Complexity: At most `2 * ((last1 - first1) + (last2 - first2)) - 1`
// comparisons and applications of each projection.
//
// Remarks: If `[first1, last1)` contains `m` elements that are equivalent to
// each other and `[first2, last2)` contains `n` elements that are equivalent to
// them, the last `max(m - n, 0)` elements from `[first1, last1)` are copied to
// the output range, in order.
//
// Reference:
// https://wg21.link/set.difference#:~:text=ranges::set_difference(I1
template <typename InputIterator1,
          typename InputIterator2,
          typename OutputIterator,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<InputIterator1>,
          typename = internal::iterator_category_t<InputIterator2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator1, Proj1>,
                                       projected<InputIterator2, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator2, Proj2>,
                                       projected<InputIterator1, Proj1>>>
constexpr auto set_difference(InputIterator1 first1,
                              InputIterator1 last1,
                              InputIterator2 first2,
                              InputIterator2 last2,
                              OutputIterator result,
                              Comp comp = {},
                              Proj1 proj1 = {},
                              Proj2 proj2 = {}) {
  // Needs to opt-in to all permutations, since std::set_difference expects
  // comp(proj1(lhs), proj2(rhs)) and comp(proj2(lhs), proj1(rhs)) to compile.
  return std::set_difference(
      first1, last1, first2, last2, result,
      internal::PermutedProjectedBinaryPredicate(comp, proj1, proj2));
}

// Preconditions: The ranges `range1` and `range2` are sorted with respect to
// `comp` and `proj1` or `proj2`, respectively. The resulting range does not
// overlap with either of the original ranges.
//
// Effects: Copies the elements of `range1` which are not present in `range2`
// to the range beginning at `result`. The elements in the constructed range are
// sorted.
//
// Returns: The end of the constructed range.
//
// Complexity: At most `2 * (size(range1) + size(range2)) - 1` comparisons and
// applications of each projection.
//
// Remarks: Stable. If `range1` contains `m` elements that are equivalent to
// each other and `range2` contains `n` elements that are equivalent to them,
// the last `max(m - n, 0)` elements from `range1` are copied to the output
// range, in order.
//
// Reference:
// https://wg21.link/set.difference#:~:text=ranges::set_difference(R1
template <typename Range1,
          typename Range2,
          typename OutputIterator,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range2>, Proj2>,
                                       projected<iterator_t<Range1>, Proj1>>>
constexpr auto set_difference(Range1&& range1,
                              Range2&& range2,
                              OutputIterator result,
                              Comp comp = {},
                              Proj1 proj1 = {},
                              Proj2 proj2 = {}) {
  return ranges::set_difference(ranges::begin(range1), ranges::end(range1),
                                ranges::begin(range2), ranges::end(range2),
                                result, std::move(comp), std::move(proj1),
                                std::move(proj2));
}

// [set.symmetric.difference] set_symmetric_difference
// Reference: https://wg21.link/set.symmetric.difference

// Preconditions: The ranges `[first1, last1)` and `[first2, last2)` are sorted
// with respect to `comp` and `proj1` or `proj2`, respectively. The resulting
// range does not overlap with either of the original ranges.
//
// Effects: Copies the elements of the range `[first1, last1)` that are not
// present in the range `[first2, last2)`, and the elements of the range
// `[first2, last2)` that are not present in the range `[first1, last1)` to the
// range beginning at `result`. The elements in the constructed range are
// sorted.
//
// Returns: The end of the constructed range.
//
// Complexity: At most `2 * ((last1 - first1) + (last2 - first2)) - 1`
// comparisons and applications of each projection.
//
// Remarks: Stable. If `[first1, last1)` contains `m` elements that are
// equivalent to each other and `[first2, last2)` contains `n` elements that are
// equivalent to them, then `|m - n|` of those elements shall be copied to the
// output range: the last `m - n` of these elements from `[first1, last1)` if
// `m > n`, and the last `n - m` of these elements from `[first2, last2)` if
// `m < n`. In either case, the elements are copied in order.
//
// Reference:
// https://wg21.link/set.symmetric.difference#:~:text=set_symmetric_difference(I1
template <typename InputIterator1,
          typename InputIterator2,
          typename OutputIterator,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<InputIterator1>,
          typename = internal::iterator_category_t<InputIterator2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator1, Proj1>,
                                       projected<InputIterator2, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<InputIterator2, Proj2>,
                                       projected<InputIterator1, Proj1>>>
constexpr auto set_symmetric_difference(InputIterator1 first1,
                                        InputIterator1 last1,
                                        InputIterator2 first2,
                                        InputIterator2 last2,
                                        OutputIterator result,
                                        Comp comp = {},
                                        Proj1 proj1 = {},
                                        Proj2 proj2 = {}) {
  // Needs to opt-in to all permutations, since std::set_symmetric_difference
  // expects comp(proj1(lhs), proj2(rhs)) and comp(proj2(lhs), proj1(rhs)) to
  // compile.
  return std::set_symmetric_difference(
      first1, last1, first2, last2, result,
      internal::PermutedProjectedBinaryPredicate(comp, proj1, proj2));
}

// Preconditions: The ranges `range1` and `range2` are sorted with respect to
// `comp` and `proj1` or `proj2`, respectively. The resulting range does not
// overlap with either of the original ranges.
//
// Effects: Copies the elements of `range1` that are not present in `range2`,
// and the elements of `range2` that are not present in `range1` to the range
// beginning at `result`. The elements in the constructed range are sorted.
//
// Returns: The end of the constructed range.
//
// Complexity: At most `2 * (size(range1) + size(range2)) - 1` comparisons and
// applications of each projection.
//
// Remarks: Stable. If `range1` contains `m` elements that are equivalent to
// each other and `range2` contains `n` elements that are equivalent to them,
// then `|m - n|` of those elements shall be copied to the output range: the
// last `m - n` of these elements from `range1` if `m > n`, and the last `n - m`
// of these elements from `range2` if `m < n`. In either case, the elements are
// copied in order.
//
// Reference:
// https://wg21.link/set.symmetric.difference#:~:text=set_symmetric_difference(R1
template <typename Range1,
          typename Range2,
          typename OutputIterator,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range2>, Proj2>,
                                       projected<iterator_t<Range1>, Proj1>>>
constexpr auto set_symmetric_difference(Range1&& range1,
                                        Range2&& range2,
                                        OutputIterator result,
                                        Comp comp = {},
                                        Proj1 proj1 = {},
                                        Proj2 proj2 = {}) {
  return ranges::set_symmetric_difference(
      ranges::begin(range1), ranges::end(range1), ranges::begin(range2),
      ranges::end(range2), result, std::move(comp), std::move(proj1),
      std::move(proj2));
}

// [alg.heap.operations] Heap operations
// Reference: https://wg21.link/alg.heap.operations

// [push.heap] push_heap
// Reference: https://wg21.link/push.heap

// Preconditions: The range `[first, last - 1)` is a valid heap with respect to
// `comp` and `proj`.
//
// Effects: Places the value in the location `last - 1` into the resulting heap
// `[first, last)`.
//
// Returns: `last`.
//
// Complexity: At most `log(last - first)` comparisons and twice as many
// projections.
//
// Reference: https://wg21.link/push.heap#:~:text=ranges::push_heap(I
template <typename RandomAccessIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<RandomAccessIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<RandomAccessIterator, Proj>,
                                       projected<RandomAccessIterator, Proj>>>
constexpr auto push_heap(RandomAccessIterator first,
                         RandomAccessIterator last,
                         Comp comp = {},
                         Proj proj = {}) {
  std::push_heap(first, last,
                 internal::ProjectedBinaryPredicate(comp, proj, proj));
  return last;
}

// Preconditions: The range `[begin(range), end(range) - 1)` is a valid heap
// with respect to `comp` and `proj`.
//
// Effects: Places the value in the location `end(range) - 1` into the resulting
// heap `range`.
//
// Returns: `end(range)`.
//
// Complexity: At most `log(size(range))` comparisons and twice as many
// projections.
//
// Reference: https://wg21.link/push.heap#:~:text=ranges::push_heap(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto push_heap(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::push_heap(ranges::begin(range), ranges::end(range),
                           std::move(comp), std::move(proj));
}

// [pop.heap] pop_heap
// Reference: https://wg21.link/pop.heap

// Preconditions: The range `[first, last)` is a valid non-empty heap with
// respect to `comp` and `proj`.
//
// Effects: Swaps the value in the location `first` with the value in the
// location `last - 1` and makes `[first, last - 1)` into a heap with respect to
// `comp` and `proj`.
//
// Returns: `last`.
//
// Complexity: At most `2 log(last - first)` comparisons and twice as many
// projections.
//
// Reference: https://wg21.link/pop.heap#:~:text=ranges::pop_heap(I
template <typename RandomAccessIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<RandomAccessIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<RandomAccessIterator, Proj>,
                                       projected<RandomAccessIterator, Proj>>>
constexpr auto pop_heap(RandomAccessIterator first,
                        RandomAccessIterator last,
                        Comp comp = {},
                        Proj proj = {}) {
  std::pop_heap(first, last,
                internal::ProjectedBinaryPredicate(comp, proj, proj));
  return last;
}

// Preconditions: `range` is a valid non-empty heap with respect to `comp` and
// `proj`.
//
// Effects: Swaps the value in the location `begin(range)` with the value in the
// location `end(range) - 1` and makes `[begin(range), end(range) - 1)` into a
// heap with respect to `comp` and `proj`.
//
// Returns: `end(range)`.
//
// Complexity: At most `2 log(size(range))` comparisons and twice as many
// projections.
//
// Reference: https://wg21.link/pop.heap#:~:text=ranges::pop_heap(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto pop_heap(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::pop_heap(ranges::begin(range), ranges::end(range),
                          std::move(comp), std::move(proj));
}

// [make.heap] make_heap
// Reference: https://wg21.link/make.heap

// Effects: Constructs a heap with respect to `comp` and `proj` out of the range
// `[first, last)`.
//
// Returns: `last`.
//
// Complexity: At most `3 * (last - first)` comparisons and twice as many
// projections.
//
// Reference: https://wg21.link/make.heap#:~:text=ranges::make_heap(I
template <typename RandomAccessIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<RandomAccessIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<RandomAccessIterator, Proj>,
                                       projected<RandomAccessIterator, Proj>>>
constexpr auto make_heap(RandomAccessIterator first,
                         RandomAccessIterator last,
                         Comp comp = {},
                         Proj proj = {}) {
  std::make_heap(first, last,
                 internal::ProjectedBinaryPredicate(comp, proj, proj));
  return last;
}

// Effects: Constructs a heap with respect to `comp` and `proj` out of `range`.
//
// Returns: `end(range)`.
//
// Complexity: At most `3 * size(range)` comparisons and twice as many
// projections.
//
// Reference: https://wg21.link/make.heap#:~:text=ranges::make_heap(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto make_heap(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::make_heap(ranges::begin(range), ranges::end(range),
                           std::move(comp), std::move(proj));
}

// [sort.heap] sort_heap
// Reference: https://wg21.link/sort.heap

// Preconditions: The range `[first, last)` is a valid heap with respect to
// `comp` and `proj`.
//
// Effects: Sorts elements in the heap `[first, last)` with respect to `comp`
// and `proj`.
//
// Returns: `last`.
//
// Complexity: At most `2 N log N` comparisons, where `N = last - first`, and
// twice as many projections.
//
// Reference: https://wg21.link/sort.heap#:~:text=ranges::sort_heap(I
template <typename RandomAccessIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<RandomAccessIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<RandomAccessIterator, Proj>,
                                       projected<RandomAccessIterator, Proj>>>
constexpr auto sort_heap(RandomAccessIterator first,
                         RandomAccessIterator last,
                         Comp comp = {},
                         Proj proj = {}) {
  std::sort_heap(first, last,
                 internal::ProjectedBinaryPredicate(comp, proj, proj));
  return last;
}

// Preconditions: `range` is a valid heap with respect to `comp` and `proj`.
//
// Effects: Sorts elements in the heap `range` with respect to `comp` and
// `proj`.
//
// Returns: `end(range)`.
//
// Complexity: At most `2 N log N` comparisons, where `N = size(range)`, and
// twice as many projections.
//
// Reference: https://wg21.link/sort.heap#:~:text=ranges::sort_heap(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto sort_heap(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::sort_heap(ranges::begin(range), ranges::end(range),
                           std::move(comp), std::move(proj));
}

// [is.heap] is_heap
// Reference: https://wg21.link/is.heap

// Returns: Whether the range `[first, last)` is a heap with respect to `comp`
// and `proj`.
//
// Complexity: Linear.
//
// Reference: https://wg21.link/is.heap#:~:text=ranges::is_heap(I
template <typename RandomAccessIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<RandomAccessIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<RandomAccessIterator, Proj>,
                                       projected<RandomAccessIterator, Proj>>>
constexpr auto is_heap(RandomAccessIterator first,
                       RandomAccessIterator last,
                       Comp comp = {},
                       Proj proj = {}) {
  return std::is_heap(first, last,
                      internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Returns: Whether `range` is a heap with respect to `comp` and `proj`.
//
// Complexity: Linear.
//
// Reference: https://wg21.link/is.heap#:~:text=ranges::is_heap(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto is_heap(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::is_heap(ranges::begin(range), ranges::end(range),
                         std::move(comp), std::move(proj));
}

// Returns: The last iterator `i` in `[first, last]` for which the range
// `[first, i)` is a heap with respect to `comp` and `proj`.
//
// Complexity: Linear.
//
// Reference: https://wg21.link/is.heap#:~:text=ranges::is_heap_until(I
template <typename RandomAccessIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<RandomAccessIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<RandomAccessIterator, Proj>,
                                       projected<RandomAccessIterator, Proj>>>
constexpr auto is_heap_until(RandomAccessIterator first,
                             RandomAccessIterator last,
                             Comp comp = {},
                             Proj proj = {}) {
  return std::is_heap_until(
      first, last, internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Returns: The last iterator `i` in `[begin(range), end(range)]` for which the
// range `[begin(range), i)` is a heap with respect to `comp` and `proj`.
//
// Complexity: Linear.
//
// Reference: https://wg21.link/is.heap#:~:text=ranges::is_heap_until(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto is_heap_until(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::is_heap_until(ranges::begin(range), ranges::end(range),
                               std::move(comp), std::move(proj));
}

// [alg.min.max] Minimum and maximum
// Reference: https://wg21.link/alg.min.max

// Returns: The smaller value. Returns the first argument when the arguments are
// equivalent.
//
// Complexity: Exactly one comparison and two applications of the projection, if
// any.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::min
template <typename T, typename Comp = ranges::less, typename Proj = identity>
constexpr const T& min(const T& a, const T& b, Comp comp = {}, Proj proj = {}) {
  return base::invoke(comp, base::invoke(proj, b), base::invoke(proj, a)) ? b
                                                                          : a;
}

// Preconditions: `!empty(ilist)`.
//
// Returns: The smallest value in the input range. Returns a copy of the
// leftmost element when several elements are equivalent to the smallest.
//
// Complexity: Exactly `size(ilist) - 1` comparisons and twice as many
// applications of the projection, if any.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::min(initializer_list
template <typename T, typename Comp = ranges::less, typename Proj = identity>
constexpr T min(std::initializer_list<T> ilist,
                Comp comp = {},
                Proj proj = {}) {
  return *std::min_element(
      ilist.begin(), ilist.end(),
      internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Preconditions: `!empty(range)`.
//
// Returns: The smallest value in the input range. Returns a copy of the
// leftmost element when several elements are equivalent to the smallest.
//
// Complexity: Exactly `size(range) - 1` comparisons and twice as many
// applications of the projection, if any.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::min(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto min(Range&& range, Comp comp = {}, Proj proj = {}) {
  return *std::min_element(
      ranges::begin(range), ranges::end(range),
      internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Returns: The larger value. Returns the first argument when the arguments are
// equivalent.
//
// Complexity: Exactly one comparison and two applications of the projection, if
// any.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::max
template <typename T, typename Comp = ranges::less, typename Proj = identity>
constexpr const T& max(const T& a, const T& b, Comp comp = {}, Proj proj = {}) {
  return base::invoke(comp, base::invoke(proj, a), base::invoke(proj, b)) ? b
                                                                          : a;
}

// Preconditions: `!empty(ilist)`.
//
// Returns: The largest value in the input range. Returns a copy of the leftmost
// element when several elements are equivalent to the largest.
//
// Complexity: Exactly `size(ilist) - 1` comparisons and twice as many
// applications of the projection, if any.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::max(initializer_list
template <typename T, typename Comp = ranges::less, typename Proj = identity>
constexpr T max(std::initializer_list<T> ilist,
                Comp comp = {},
                Proj proj = {}) {
  return *std::max_element(
      ilist.begin(), ilist.end(),
      internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Preconditions: `!empty(range)`.
//
// Returns: The largest value in the input range. Returns a copy of the leftmost
// element when several elements are equivalent to the smallest.
//
// Complexity: Exactly `size(range) - 1` comparisons and twice as many
// applications of the projection, if any.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::max(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto max(Range&& range, Comp comp = {}, Proj proj = {}) {
  return *std::max_element(
      ranges::begin(range), ranges::end(range),
      internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Returns: `{b, a}` if `b` is smaller than `a`, and `{a, b}` otherwise.
//
// Complexity: Exactly one comparison and two applications of the projection, if
// any.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::minmax
template <typename T, typename Comp = ranges::less, typename Proj = identity>
constexpr auto minmax(const T& a, const T& b, Comp comp = {}, Proj proj = {}) {
  return std::minmax(a, b,
                     internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Preconditions: `!empty(ilist)`.
//
// Returns: Let `X` be the return type. Returns `X{x, y}`, where `x` is a copy
// of the leftmost element with the smallest value and `y` a copy of the
// rightmost element with the largest value in the input range.
//
// Complexity: At most `(3/2) size(ilist)` applications of the corresponding
// predicate and twice as many applications of the projection, if any.
//
// Reference:
// https://wg21.link/alg.min.max#:~:text=ranges::minmax(initializer_list
template <typename T, typename Comp = ranges::less, typename Proj = identity>
constexpr auto minmax(std::initializer_list<T> ilist,
                      Comp comp = {},
                      Proj proj = {}) {
  auto it =
      std::minmax_element(ranges::begin(ilist), ranges::end(ilist),
                          internal::ProjectedBinaryPredicate(comp, proj, proj));
  return std::pair<T, T>{*it.first, *it.second};
}

// Preconditions: `!empty(range)`.
//
// Returns: Let `X` be the return type. Returns `X{x, y}`, where `x` is a copy
// of the leftmost element with the smallest value and `y` a copy of the
// rightmost element with the largest value in the input range.
//
// Complexity: At most `(3/2) size(range)` applications of the corresponding
// predicate and twice as many applications of the projection, if any.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::minmax(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto minmax(Range&& range, Comp comp = {}, Proj proj = {}) {
  using T = range_value_t<Range>;
  auto it =
      std::minmax_element(ranges::begin(range), ranges::end(range),
                          internal::ProjectedBinaryPredicate(comp, proj, proj));
  return std::pair<T, T>{*it.first, *it.second};
}

// Returns: The first iterator i in the range `[first, last)` such that for
// every iterator `j` in the range `[first, last)`,
// `bool(invoke(comp, invoke(proj, *j), invoke(proj, *i)))` is `false`. Returns
// `last` if `first == last`.
//
// Complexity: Exactly `max(last - first - 1, 0)` comparisons and twice as
// many projections.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::min_element(I
template <typename ForwardIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<ForwardIterator, Proj>,
                                       projected<ForwardIterator, Proj>>>
constexpr auto min_element(ForwardIterator first,
                           ForwardIterator last,
                           Comp comp = {},
                           Proj proj = {}) {
  return std::min_element(first, last,
                          internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Returns: The first iterator i in `range` such that for every iterator `j` in
// `range`, `bool(invoke(comp, invoke(proj, *j), invoke(proj, *i)))` is `false`.
// Returns `end(range)` if `empty(range)`.
//
// Complexity: Exactly `max(size(range) - 1, 0)` comparisons and twice as many
// projections.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::min_element(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto min_element(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::min_element(ranges::begin(range), ranges::end(range),
                             std::move(comp), std::move(proj));
}

// Returns: The first iterator i in the range `[first, last)` such that for
// every iterator `j` in the range `[first, last)`,
// `bool(invoke(comp, invoke(proj, *i), invoke(proj, *j)))` is `false`.
// Returns `last` if `first == last`.
//
// Complexity: Exactly `max(last - first - 1, 0)` comparisons and twice as
// many projections.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::max_element(I
template <typename ForwardIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<ForwardIterator, Proj>,
                                       projected<ForwardIterator, Proj>>>
constexpr auto max_element(ForwardIterator first,
                           ForwardIterator last,
                           Comp comp = {},
                           Proj proj = {}) {
  return std::max_element(first, last,
                          internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Returns: The first iterator i in `range` such that for every iterator `j`
// in `range`, `bool(invoke(comp, invoke(proj, *j), invoke(proj, *j)))` is
// `false`. Returns `end(range)` if `empty(range)`.
//
// Complexity: Exactly `max(size(range) - 1, 0)` comparisons and twice as many
// projections.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::max_element(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto max_element(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::max_element(ranges::begin(range), ranges::end(range),
                             std::move(comp), std::move(proj));
}

// Returns: `{first, first}` if `[first, last)` is empty, otherwise `{m, M}`,
// where `m` is the first iterator in `[first, last)` such that no iterator in
// the range refers to a smaller element, and where `M` is the last iterator
// in
// `[first, last)` such that no iterator in the range refers to a larger
// element.
//
// Complexity: Let `N` be `last - first`. At most `max(3/2 (N − 1), 0)`
// comparisons and twice as many applications of the projection, if any.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::minmax_element(I
template <typename ForwardIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<ForwardIterator, Proj>,
                                       projected<ForwardIterator, Proj>>>
constexpr auto minmax_element(ForwardIterator first,
                              ForwardIterator last,
                              Comp comp = {},
                              Proj proj = {}) {
  return std::minmax_element(
      first, last, internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Returns: `{begin(range), begin(range)}` if `range` is empty, otherwise
// `{m, M}`, where `m` is the first iterator in `range` such that no iterator
// in the range refers to a smaller element, and where `M` is the last
// iterator in `range` such that no iterator in the range refers to a larger
// element.
//
// Complexity: Let `N` be `size(range)`. At most `max(3/2 (N − 1), 0)`
// comparisons and twice as many applications of the projection, if any.
//
// Reference: https://wg21.link/alg.min.max#:~:text=ranges::minmax_element(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto minmax_element(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::minmax_element(ranges::begin(range), ranges::end(range),
                                std::move(comp), std::move(proj));
}

// [alg.clamp] Bounded value
// Reference: https://wg21.link/alg.clamp

// Preconditions: `bool(invoke(comp, invoke(proj, hi), invoke(proj, lo)))` is
// `false`.
//
// Returns: `lo` if `bool(invoke(comp, invoke(proj, v), invoke(proj, lo)))` is
// `true`, `hi` if `bool(invoke(comp, invoke(proj, hi), invoke(proj, v)))` is
// `true`, otherwise `v`.
//
// Complexity: At most two comparisons and three applications of the
// projection.
//
// Reference: https://wg21.link/alg.clamp#:~:text=ranges::clamp
template <typename T, typename Comp = ranges::less, typename Proj = identity>
constexpr const T& clamp(const T& v,
                         const T& lo,
                         const T& hi,
                         Comp comp = {},
                         Proj proj = {}) {
  auto&& projected_v = base::invoke(proj, v);
  if (base::invoke(comp, projected_v, base::invoke(proj, lo)))
    return lo;

  return base::invoke(comp, base::invoke(proj, hi), projected_v) ? hi : v;
}

// [alg.lex.comparison] Lexicographical comparison
// Reference: https://wg21.link/alg.lex.comparison

// Returns: `true` if and only if the sequence of elements defined by the range
// `[first1, last1)` is lexicographically less than the sequence of elements
// defined by the range `[first2, last2)`.
//
// Complexity: At most `2 min(last1 - first1, last2 - first2)` applications of
// the corresponding comparison and each projection, if any.
//
// Remarks: If two sequences have the same number of elements and their
// corresponding elements (if any) are equivalent, then neither sequence is
// lexicographically less than the other. If one sequence is a proper prefix of
// the other, then the shorter sequence is lexicographically less than the
// longer sequence. Otherwise, the lexicographical comparison of the sequences
// yields the same result as the comparison of the first corresponding pair of
// elements that are not equivalent.
//
// Reference:
// https://wg21.link/alg.lex.comparison#:~:text=lexicographical_compare(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>,
          typename = indirect_result_t<Comp&,
                                       projected<ForwardIterator1, Proj1>,
                                       projected<ForwardIterator2, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<ForwardIterator2, Proj2>,
                                       projected<ForwardIterator1, Proj1>>>
constexpr bool lexicographical_compare(ForwardIterator1 first1,
                                       ForwardIterator1 last1,
                                       ForwardIterator2 first2,
                                       ForwardIterator2 last2,
                                       Comp comp = {},
                                       Proj1 proj1 = {},
                                       Proj2 proj2 = {}) {
  for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
    auto&& projected_first1 = base::invoke(proj1, *first1);
    auto&& projected_first2 = base::invoke(proj2, *first2);
    if (base::invoke(comp, projected_first1, projected_first2))
      return true;
    if (base::invoke(comp, projected_first2, projected_first1))
      return false;
  }

  // `first2 != last2` is equivalent to `first1 == last1 && first2 != last2`
  // here, since we broke out of the loop above.
  return first2 != last2;
}

// Returns: `true` if and only if the sequence of elements defined by `range1`
//  is lexicographically less than the sequence of elements defined by `range2`.
//
// Complexity: At most `2 min(size(range1), size(range2))` applications of the
// corresponding comparison and each projection, if any.
//
// Remarks: If two sequences have the same number of elements and their
// corresponding elements (if any) are equivalent, then neither sequence is
// lexicographically less than the other. If one sequence is a proper prefix of
// the other, then the shorter sequence is lexicographically less than the
// longer sequence. Otherwise, the lexicographical comparison of the sequences
// yields the same result as the comparison of the first corresponding pair of
// elements that are not equivalent.
//
// Reference:
// https://wg21.link/alg.lex.comparison#:~:text=lexicographical_compare(R1
template <typename Range1,
          typename Range2,
          typename Comp = ranges::less,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range2>, Proj2>,
                                       projected<iterator_t<Range1>, Proj1>>>
constexpr bool lexicographical_compare(Range1&& range1,
                                       Range2&& range2,
                                       Comp comp = {},
                                       Proj1 proj1 = {},
                                       Proj2 proj2 = {}) {
  return ranges::lexicographical_compare(
      ranges::begin(range1), ranges::end(range1), ranges::begin(range2),
      ranges::end(range2), std::move(comp), std::move(proj1), std::move(proj2));
}

// [alg.permutation.generators] Permutation generators
// Reference: https://wg21.link/alg.permutation.generators

// Effects: Takes a sequence defined by the range `[first, last)` and transforms
// it into the next permutation. The next permutation is found by assuming that
// the set of all permutations is lexicographically sorted with respect to
// `comp` and `proj`. If no such permutation exists, transforms the sequence
// into the first permutation; that is, the ascendingly-sorted one.
//
// Returns: `true` if a next permutation was found and otherwise `false`.
//
// Complexity: At most `(last - first) / 2` swaps.
//
// Reference:
// https://wg21.link/alg.permutation.generators#:~:text=next_permutation(I
template <typename BidirectionalIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<BidirectionalIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<BidirectionalIterator, Proj>,
                                       projected<BidirectionalIterator, Proj>>>
constexpr auto next_permutation(BidirectionalIterator first,
                                BidirectionalIterator last,
                                Comp comp = {},
                                Proj proj = {}) {
  return std::next_permutation(
      first, last, internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Effects: Takes a sequence defined by `range` and transforms it into the next
// permutation. The next permutation is found by assuming that the set of all
// permutations is lexicographically sorted with respect to `comp` and `proj`.
// If no such permutation exists, transforms the sequence into the first
// permutation; that is, the ascendingly-sorted one.
//
// Returns: `true` if a next permutation was found and otherwise `false`.
//
// Complexity: At most `size(range) / 2` swaps.
//
// Reference:
// https://wg21.link/alg.permutation.generators#:~:text=next_permutation(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto next_permutation(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::next_permutation(ranges::begin(range), ranges::end(range),
                                  std::move(comp), std::move(proj));
}

// Effects: Takes a sequence defined by the range `[first, last)` and transforms
// it into the previous permutation. The previous permutation is found by
// assuming that the set of all permutations is lexicographically sorted with
// respect to `comp` and `proj`. If no such permutation exists, transforms the
// sequence into the last permutation; that is, the decreasingly-sorted one.
//
// Returns: `true` if a next permutation was found and otherwise `false`.
//
// Complexity: At most `(last - first) / 2` swaps.
//
// Reference:
// https://wg21.link/alg.permutation.generators#:~:text=prev_permutation(I
template <typename BidirectionalIterator,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::iterator_category_t<BidirectionalIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<BidirectionalIterator, Proj>,
                                       projected<BidirectionalIterator, Proj>>>
constexpr auto prev_permutation(BidirectionalIterator first,
                                BidirectionalIterator last,
                                Comp comp = {},
                                Proj proj = {}) {
  return std::prev_permutation(
      first, last, internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Effects: Takes a sequence defined by `range` and transforms it into the
// previous permutation. The previous permutation is found by assuming that the
// set of all permutations is lexicographically sorted with respect to `comp`
// and `proj`. If no such permutation exists, transforms the sequence into the
// last permutation; that is, the decreasingly-sorted one.
//
// Returns: `true` if a previous permutation was found and otherwise `false`.
//
// Complexity: At most `size(range) / 2` swaps.
//
// Reference:
// https://wg21.link/alg.permutation.generators#:~:text=prev_permutation(R
template <typename Range,
          typename Comp = ranges::less,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto prev_permutation(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::prev_permutation(ranges::begin(range), ranges::end(range),
                                  std::move(comp), std::move(proj));
}

}  // namespace ranges

}  // namespace base

#endif  // BASE_RANGES_ALGORITHM_H_
