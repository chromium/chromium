// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_RANGES_ALGORITHM_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_RANGES_ALGORITHM_H_

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/cxx20_is_constant_evaluated.h"
#include "partition_alloc/partition_alloc_base/ranges/functional.h"
#include "partition_alloc/partition_alloc_base/ranges/ranges.h"

namespace partition_alloc::internal::base {

namespace internal {

// Returns a transformed version of the unary predicate `pred` applying `proj`
// to its argument before invoking `pred` on it.
// Ensures that the return type of `invoke(pred, ...)` is convertible to bool.
template <typename Pred, typename Proj>
constexpr auto ProjectedUnaryPredicate(Pred& pred, Proj& proj) noexcept {
  return [&pred, &proj](auto&& arg) -> bool {
    return std::invoke(pred,
                       std::invoke(proj, std::forward<decltype(arg)>(arg)));
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
//   typename =
//       std::indirect_result_t<Pred&,
//                              std::projected<iterator_t<Range1>, Proj1>,
//                              std::projected<iterator_t<Range2>, Proj2>>
//
// Ensures that the return type of `invoke(pred, ...)` is convertible to bool.
template <typename Pred, typename Proj1, typename Proj2, bool kPermute = false>
class BinaryPredicateProjector {
 public:
  constexpr BinaryPredicateProjector(Pred& pred, Proj1& proj1, Proj2& proj2)
      : pred_(pred), proj1_(proj1), proj2_(proj2) {}

 private:
  template <typename ProjT, typename ProjU, typename T, typename U>
  using InvokeResult = std::invoke_result_t<Pred&,
                                            std::invoke_result_t<ProjT&, T&&>,
                                            std::invoke_result_t<ProjU&, U&&>>;

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
    return std::invoke(pred_, std::invoke(projs.first, std::forward<T>(lhs)),
                       std::invoke(projs.second, std::forward<U>(rhs)));
  }

 private:
  // This field is not a raw_ref<> because it was filtered by the rewriter for:
  // #constexpr-ctor-field-initializer
  Pred& pred_;
  // This field is not a raw_ref<> because it was filtered by the rewriter for:
  // #constexpr-ctor-field-initializer
  Proj1& proj1_;
  // This field is not a raw_ref<> because it was filtered by the rewriter for:
  // #constexpr-ctor-field-initializer
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
          typename Proj = std::identity,
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
  if (first == last) {
    return last;
  }

  for (ForwardIterator next = first; ++next != last; ++first) {
    if (std::invoke(pred, std::invoke(proj, *first),
                    std::invoke(proj, *next))) {
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
          typename Proj = std::identity,
          typename = internal::range_category_t<Range>>
constexpr auto adjacent_find(Range&& range, Pred pred = {}, Proj proj = {}) {
  return ranges::adjacent_find(ranges::begin(range), ranges::end(range),
                               std::move(pred), std::move(proj));
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
template <
    typename InputIterator,
    typename OutputIterator,
    typename UnaryOperation,
    typename Proj = std::identity,
    typename = internal::iterator_category_t<InputIterator>,
    typename = internal::iterator_category_t<OutputIterator>,
    typename = std::indirect_result_t<UnaryOperation&,
                                      std::projected<InputIterator, Proj>>>
constexpr auto transform(InputIterator first1,
                         InputIterator last1,
                         OutputIterator result,
                         UnaryOperation op,
                         Proj proj = {}) {
  return std::transform(first1, last1, result, [&op, &proj](auto&& arg) {
    return std::invoke(op, std::invoke(proj, std::forward<decltype(arg)>(arg)));
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
template <
    typename Range,
    typename OutputIterator,
    typename UnaryOperation,
    typename Proj = std::identity,
    typename = internal::range_category_t<Range>,
    typename = internal::iterator_category_t<OutputIterator>,
    typename = std::indirect_result_t<UnaryOperation&,
                                      std::projected<iterator_t<Range>, Proj>>>
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
template <
    typename ForwardIterator1,
    typename ForwardIterator2,
    typename OutputIterator,
    typename BinaryOperation,
    typename Proj1 = std::identity,
    typename Proj2 = std::identity,
    typename = internal::iterator_category_t<ForwardIterator1>,
    typename = internal::iterator_category_t<ForwardIterator2>,
    typename = internal::iterator_category_t<OutputIterator>,
    typename = std::indirect_result_t<BinaryOperation&,
                                      std::projected<ForwardIterator1, Proj1>,
                                      std::projected<ForwardIterator2, Proj2>>>
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
        return std::invoke(
            binary_op, std::invoke(proj1, std::forward<decltype(lhs)>(lhs)),
            std::invoke(proj2, std::forward<decltype(rhs)>(rhs)));
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
          typename Proj1 = std::identity,
          typename Proj2 = std::identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename =
              std::indirect_result_t<BinaryOperation&,
                                     std::projected<iterator_t<Range1>, Proj1>,
                                     std::projected<iterator_t<Range2>, Proj2>>>
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
          typename Proj = std::identity,
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
          typename Proj = std::identity,
          typename = internal::range_category_t<Range>>
constexpr auto remove_if(Range&& range, Predicate pred, Proj proj = {}) {
  return ranges::remove_if(ranges::begin(range), ranges::end(range),
                           std::move(pred), std::move(proj));
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
          typename Proj = std::identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto lower_bound(ForwardIterator first,
                           ForwardIterator last,
                           const T& value,
                           Comp comp = {},
                           Proj proj = {}) {
  // The second arg is guaranteed to be `value`, so we'll simply apply the
  // std::identity projection.
  std::identity value_proj;
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
          typename Proj = std::identity,
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
          typename Proj = std::identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto upper_bound(ForwardIterator first,
                           ForwardIterator last,
                           const T& value,
                           Comp comp = {},
                           Proj proj = {}) {
  // The first arg is guaranteed to be `value`, so we'll simply apply the
  // std::identity projection.
  std::identity value_proj;
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
          typename Proj = std::identity,
          typename = internal::range_category_t<Range>>
constexpr auto upper_bound(Range&& range,
                           const T& value,
                           Comp comp = {},
                           Proj proj = {}) {
  return ranges::upper_bound(ranges::begin(range), ranges::end(range), value,
                             std::move(comp), std::move(proj));
}

}  // namespace ranges

}  // namespace partition_alloc::internal::base

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_RANGES_ALGORITHM_H_
