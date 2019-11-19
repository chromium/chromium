// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_GMOCK_CALLBACK_SUPPORT_H_
#define BASE_TEST_GMOCK_CALLBACK_SUPPORT_H_

#include <functional>
#include <tuple>
#include <utility>

#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
namespace test {

// Matchers for base::{Once,Repeating}Callback and
// base::{Once,Repeating}Closure.

MATCHER(IsNullCallback, "a null callback") {
  return (arg.is_null());
}

MATCHER(IsNotNullCallback, "a non-null callback") {
  return (!arg.is_null());
}

// The RunClosure<N>() action invokes Run() method on the N-th (0-based)
// argument of the mock function.

ACTION_TEMPLATE(RunClosure,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  ::testing::get<k>(args).Run();
}

ACTION_P(RunClosure, closure) {
  closure.Run();
}

ACTION_TEMPLATE(RunOnceClosure,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  std::move(::testing::get<k>(args)).Run();
}

ACTION_P(RunOnceClosure, closure) {
  std::move(closure).Run();
}

// Implementation of the Run(Once)Callback gmock action.
//
// The RunCallback<N>(p1, p2, ..., p_k) action invokes Run() method on the N-th
// (0-based) argument of the mock function, with arguments p1, p2, ..., p_k.
//
// Notes:
//
//   1. The arguments are passed by value by default.  If you need to
//   pass an argument by reference, wrap it inside ByRef().  For example,
//
//     RunCallback<1>(5, string("Hello"), ByRef(foo))
//
//   passes 5 and string("Hello") by value, and passes foo by reference.
//
//   2. If the callback takes an argument by reference but ByRef() is
//   not used, it will receive the reference to a copy of the value,
//   instead of the original value.  For example, when the 0-th
//   argument of the callback takes a const string&, the action
//
//     RunCallback<0>(string("Hello"))
//
//   makes a copy of the temporary string("Hello") object and passes a
//   reference of the copy, instead of the original temporary object,
//   to the callback.  This makes it easy for a user to define an
//   RunCallback action from temporary values and have it performed later.

// TODO(crbug.com/752720): Simplify using std::apply once C++17 is available.
template <typename CallbackFunc, typename ArgTuple, size_t... I>
decltype(auto) RunCallbackUnwrapped(CallbackFunc&& f,
                                    ArgTuple&& t,
                                    std::index_sequence<I...>) {
  return std::move(f).Run(std::get<I>(t)...);
}

template <size_t I, typename... Vals>
struct RunOnceCallbackAction {
  std::tuple<Vals...> vals;

  template <typename... Args>
  decltype(auto) operator()(Args&&... args) {
    constexpr size_t size = std::tuple_size<decltype(vals)>::value;
    return RunCallbackUnwrapped(
        std::get<I>(std::forward_as_tuple(std::forward<Args>(args)...)),
        std::move(vals), std::make_index_sequence<size>{});
  }
};

template <size_t I, typename... Vals>
RunOnceCallbackAction<I, std::decay_t<Vals>...> RunOnceCallback(
    Vals&&... vals) {
  return {std::forward_as_tuple(std::forward<Vals>(vals)...)};
}

template <size_t I, typename... Vals>
RunOnceCallbackAction<I, std::decay_t<Vals>...> RunCallback(Vals&&... vals) {
  return {std::forward_as_tuple(std::forward<Vals>(vals)...)};
}

}  // namespace test
}  // namespace base

#endif  // BASE_TEST_GMOCK_CALLBACK_SUPPORT_H_
