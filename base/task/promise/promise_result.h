// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_PROMISE_RESULT_H_
#define BASE_TASK_PROMISE_PROMISE_RESULT_H_

#include <type_traits>

#include "base/task/promise/promise_value.h"

namespace base {

// An optional return type from a promise callback, which allows the callback to
// decide whether or not it should reject.  E.g.
//
// enum class Error { kReason };
//
// PromiseResult<int, Error> MyFn() {
//   int result;
//   ...
//   if (error)
//     return Error::kReason;
//   return result;
// }
//
// If ResolveType and RejectType are distinct PromiseResult's constructor will
// accept them.  E.g.
//
// PromiseResult<int, std::string> pr(123);       // Resolve
// PromiseResult<int, std::string> pr("whoops");  // Reject
//
// If ResolveType and RejectType are the same you need to use Resolved<> and
// Rejected<> to disambiguate.  E.g.
//
// PromiseResult<int, int> pr(Resolved{123});     // Resolve
// PromiseResult<int, int> pr(Rejected{123});     // Reject
//
// PromiseResult<ResolveType, RejectType> has a constructor that accepts
// Promise<ResolveType, RejectType>.
template <typename ResolveType, typename RejectType = NoReject>
class PromiseResult {
 public:
  template <typename ResolveT = ResolveType,
            typename RejectT = RejectType,
            class Enable = std::enable_if<std::is_void<ResolveT>::value ^
                                          std::is_void<RejectT>::value>>
  PromiseResult() : PromiseResult(typename Analyze<void>::TagType()) {}

  template <typename T>
  PromiseResult(T&& t)
      : PromiseResult(typename Analyze<std::decay_t<T>>::TagType(),
                      std::forward<T>(t)) {}

  PromiseResult(PromiseResult&& other) noexcept = default;

  PromiseResult(const PromiseResult&) = delete;
  PromiseResult& operator=(const PromiseResult&) = delete;

  internal::PromiseValue& value() { return value_; }

 private:
  struct IsWrapped {};
  struct IsResolved {};
  struct IsRejected {};
  struct IsPromise {};

  // Helper that assigns one of the above tags for |T|.
  template <typename T>
  struct Analyze {
    using DecayedT = std::decay_t<T>;

    static constexpr bool is_resolve =
        std::is_convertible<DecayedT, std::decay_t<ResolveType>>::value;

    static constexpr bool is_reject =
        std::is_convertible<DecayedT, std::decay_t<RejectType>>::value;

    static_assert(!is_reject || !std::is_same<DecayedT, NoReject>::value,
                  "A NoReject promise can't reject");

    static_assert(!std::is_same<std::decay_t<ResolveType>,
                                std::decay_t<RejectType>>::value,
                  "Ambiguous because ResolveType and RejectType are the same");

    static_assert(is_resolve || is_reject,
                  "Argument matches neither resolve nor reject type.");

    static_assert(
        is_resolve != is_reject && (is_resolve || is_reject),
        "Ambiguous because argument matches both ResolveType and RejectType");

    using TagType = std::conditional_t<is_resolve, IsResolved, IsRejected>;
  };

  template <typename T>
  struct Analyze<Resolved<T>> {
    using TagType = IsWrapped;
    static_assert(std::is_same<ResolveType, T>::value,
                  "T in Resolved<T> is not ResolveType");
  };

  template <typename T>
  struct Analyze<Rejected<T>> {
    static_assert(std::is_same<RejectType, T>::value,
                  "T in Rejected<T> is not RejectType");

    static_assert(!std::is_same<RejectType, NoReject>::value,
                  "A NoReject promise can't reject");
    using TagType = IsWrapped;
  };

  template <typename ResolveT, typename RejectT>
  struct Analyze<Promise<ResolveT, RejectT>> {
    using TagType = IsPromise;

    static_assert(std::is_same<ResolveT, ResolveType>::value,
                  "Promise resolve types don't match");

    static_assert(std::is_same<RejectT, RejectType>::value,
                  "Promise reject types don't match");
  };

  template <typename... Args>
  PromiseResult(IsResolved, Args&&... args)
      : value_(in_place_type_t<Resolved<ResolveType>>(),
               std::forward<Args>(args)...) {}

  template <typename... Args>
  PromiseResult(IsRejected, Args&&... args)
      : value_(in_place_type_t<Rejected<RejectType>>(),
               std::forward<Args>(args)...) {}

  PromiseResult(IsPromise, const Promise<ResolveType, RejectType>& promise)
      : value_(promise.abstract_promise_) {}

  template <typename T>
  PromiseResult(IsWrapped, T&& t) : value_(std::forward<T>(t)) {}

  internal::PromiseValue value_;
};

}  // namespace base

#endif  // BASE_TASK_PROMISE_PROMISE_RESULT_H_
