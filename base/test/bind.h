// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_BIND_H_
#define BASE_TEST_BIND_H_

#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_piece.h"

namespace base {

class Location;

namespace internal {

template <typename Callable,
          typename Signature = decltype(&Callable::operator())>
struct HasConstCallOperatorImpl : std::false_type {};

template <typename Callable, typename R, typename... Args>
struct HasConstCallOperatorImpl<Callable, R (Callable::*)(Args...) const>
    : std::true_type {};

template <typename Callable>
constexpr bool HasConstCallOperator =
    HasConstCallOperatorImpl<std::decay_t<Callable>>::value;

template <typename F, typename Signature>
struct BindLambdaHelper;

template <typename F, typename R, typename... Args>
struct BindLambdaHelper<F, R(Args...)> {
  static R Run(const std::decay_t<F>& f, Args... args) {
    return f(std::forward<Args>(args)...);
  }

  static R RunOnce(std::decay_t<F>&& f, Args... args) {
    return f(std::forward<Args>(args)...);
  }
};

}  // namespace internal

// A variant of BindRepeating() that can bind capturing lambdas for testing.
// This doesn't support extra arguments binding as the lambda itself can do.
template <typename Lambda,
          std::enable_if_t<internal::HasConstCallOperator<Lambda>>* = nullptr>
decltype(auto) BindLambdaForTesting(Lambda&& lambda) {
  using Signature = internal::ExtractCallableRunType<std::decay_t<Lambda>>;
  return BindRepeating(&internal::BindLambdaHelper<Lambda, Signature>::Run,
                       std::forward<Lambda>(lambda));
}

// A variant of BindOnce() that can bind mutable capturing lambdas for
// testing. This doesn't support extra arguments binding as the lambda itself
// can do. Since a mutable lambda potentially can invalidate its state after
// being run once, this method returns a OnceCallback instead of a
// RepeatingCallback.
template <typename Lambda,
          std::enable_if_t<!internal::HasConstCallOperator<Lambda>>* = nullptr>
decltype(auto) BindLambdaForTesting(Lambda&& lambda) {
  static_assert(
      std::is_rvalue_reference<Lambda&&>() &&
          !std::is_const<std::remove_reference_t<Lambda>>(),
      "BindLambdaForTesting requires non-const rvalue for mutable lambda "
      "binding. I.e.: base::BindLambdaForTesting(std::move(lambda)).");
  using Signature = internal::ExtractCallableRunType<std::decay_t<Lambda>>;
  return BindOnce(&internal::BindLambdaHelper<Lambda, Signature>::RunOnce,
                  std::move(lambda));
}

// Returns a closure that fails on destruction if it hasn't been run.
OnceClosure MakeExpectedRunClosure(const Location& location,
                                   StringPiece message = StringPiece());
RepeatingClosure MakeExpectedRunAtLeastOnceClosure(
    const Location& location,
    StringPiece message = StringPiece());

// Returns a closure that fails the test if run.
RepeatingClosure MakeExpectedNotRunClosure(const Location& location,
                                           StringPiece message = StringPiece());

}  // namespace base

#endif  // BASE_TEST_BIND_H_
