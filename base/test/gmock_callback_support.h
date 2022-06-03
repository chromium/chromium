// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_GMOCK_CALLBACK_SUPPORT_H_
#define BASE_TEST_GMOCK_CALLBACK_SUPPORT_H_

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

namespace internal {

// Small helper to get the `I`th argument.
template <size_t I, typename... Args>
decltype(auto) get(Args&&... args) {
  return std::get<I>(std::forward_as_tuple(std::forward<Args>(args)...));
}

// Invokes `cb` with the arguments stored in `tuple`. Both `cb` and `tuple` are
// perfectly forwarded, allowing callers to specify whether they should be
// passed by move or copy.
template <typename Callback, typename Tuple, size_t... Is>
decltype(auto) RunImpl(Callback&& cb,
                       Tuple&& tuple,
                       std::index_sequence<Is...>) {
  return std::forward<Callback>(cb).Run(
      std::get<Is>(std::forward<Tuple>(tuple))...);
}

// Invokes `cb` with the arguments stored in `tuple`. Both `cb` and `tuple` are
// perfectly forwarded, allowing callers to specify whether they should be
// passed by move or copy. Needs to dispatch to the three arguments version to
// be able to construct a `std::index_sequence` of the corresponding size.
template <typename Callback, typename Tuple>
decltype(auto) RunImpl(Callback&& cb, Tuple&& tuple) {
  return RunImpl(std::forward<Callback>(cb), std::forward<Tuple>(tuple),
                 std::make_index_sequence<
                     std::tuple_size<std::remove_reference_t<Tuple>>::value>());
}

// Invoked when the arguments to a OnceCallback are copy constructible. In this
// case the returned lambda will pass the arguments to the provided callback by
// copy, allowing it to be used multiple times.
template <size_t I,
          typename Tuple,
          std::enable_if_t<std::is_copy_constructible<Tuple>::value, int> = 0>
auto RunOnceCallbackImpl(Tuple&& tuple) {
  return
      [tuple = std::forward<Tuple>(tuple)](auto&&... args) -> decltype(auto) {
        return RunImpl(std::move(internal::get<I>(args...)), tuple);
      };
}

// Invoked when the arguments to a OnceCallback are not copy constructible. In
// this case the returned lambda will pass the arguments to the provided
// callback by move, allowing it to be only used once.
template <size_t I,
          typename Tuple,
          std::enable_if_t<!std::is_copy_constructible<Tuple>::value, int> = 0>
auto RunOnceCallbackImpl(Tuple&& tuple) {
  // Mock actions need to be copyable, but `tuple` is not. Wrap it in in a
  // `scoped_refptr` to allow it to be copied.
  auto tuple_ptr = base::MakeRefCounted<base::RefCountedData<Tuple>>(
      std::forward<Tuple>(tuple));
  return [tuple_ptr =
              std::move(tuple_ptr)](auto&&... args) mutable -> decltype(auto) {
    // Since running the action will move out of the arguments, `tuple_ptr` is
    // nulled out, so that attempting to run it twice will result in a run-time
    // crash.
    return RunImpl(std::move(internal::get<I>(args...)),
                   std::move(std::exchange(tuple_ptr, nullptr)->data));
  };
}

// Invoked for RepeatingCallbacks. In this case the returned lambda will pass
// the arguments to the provided callback by copy, allowing it to be used
// multiple times. Move-only arguments are not supported.
template <size_t I, typename Tuple>
auto RunRepeatingCallbackImpl(Tuple&& tuple) {
  return
      [tuple = std::forward<Tuple>(tuple)](auto&&... args) -> decltype(auto) {
        return RunImpl(internal::get<I>(args...), tuple);
      };
}

}  // namespace internal

namespace test {

// Matchers for base::{Once,Repeating}Callback and
// base::{Once,Repeating}Closure.
MATCHER(IsNullCallback, "a null callback") {
  return (arg.is_null());
}

MATCHER(IsNotNullCallback, "a non-null callback") {
  return (!arg.is_null());
}

// The Run[Once]Closure() action invokes the Run() method on the closure
// provided when the action is constructed. Function arguments passed when the
// action is run will be ignored.
ACTION_P(RunClosure, closure) {
  closure.Run();
}

// This action can be invoked at most once. Any further invocation will trigger
// a CHECK failure.
inline auto RunOnceClosure(base::OnceClosure cb) {
  // Mock actions need to be copyable, but OnceClosure is not. Wrap the closure
  // in a base::RefCountedData<> to allow it to be copied.
  using RefCountedOnceClosure = base::RefCountedData<base::OnceClosure>;
  scoped_refptr<RefCountedOnceClosure> copyable_cb =
      base::MakeRefCounted<RefCountedOnceClosure>(std::move(cb));
  return [copyable_cb](auto&&...) {
    CHECK(copyable_cb->data);
    std::move(copyable_cb->data).Run();
  };
}

// The Run[Once]Closure<N>() action invokes the Run() method on the N-th
// (0-based) argument of the mock function.
template <size_t I>
auto RunClosure() {
  return [](auto&&... args) -> decltype(auto) {
    return internal::get<I>(args...).Run();
  };
}

template <size_t I>
auto RunOnceClosure() {
  return [](auto&&... args) -> decltype(auto) {
    return std::move(internal::get<I>(args...)).Run();
  };
}

// The Run[Once]Callback<N>(p1, p2, ..., p_k) action invokes the Run() method on
// the N-th (0-based) argument of the mock function, with arguments p1, p2, ...,
// p_k.
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
//
//   3. In order to facilitate re-use of the `RunOnceCallback()` action,
//   the arguments are copied during each run if possible. If this can't
//   be done (e.g. one of the arguments is move-only), the arguments will
//   be passed by move. However, since moving potentially invalidates the
//   arguments, the resulting action is only allowed to run once in this
//   case. Attempting to run it twice will result in a runtime crash.
//   Using move-only arguments with `RunCallback()` is not supported.
template <size_t I, typename... RunArgs>
auto RunOnceCallback(RunArgs&&... run_args) {
  return internal::RunOnceCallbackImpl<I>(
      std::make_tuple(std::forward<RunArgs>(run_args)...));
}

template <size_t I, typename... RunArgs>
auto RunCallback(RunArgs&&... run_args) {
  return internal::RunRepeatingCallbackImpl<I>(
      std::make_tuple(std::forward<RunArgs>(run_args)...));
}

}  // namespace test
}  // namespace base

#endif  // BASE_TEST_GMOCK_CALLBACK_SUPPORT_H_
