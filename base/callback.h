// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NOTE: Header files that do not require the full definition of
// base::{Once,Repeating}Callback or base::{Once,Repeating}Closure should
// #include "base/callback_forward.h" instead of this file.

#ifndef BASE_CALLBACK_H_
#define BASE_CALLBACK_H_

#include <stddef.h>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_internal.h"
#include "base/notreached.h"

// -----------------------------------------------------------------------------
// Usage documentation
// -----------------------------------------------------------------------------
//
// Overview:
// A callback is similar in concept to a function pointer: it wraps a runnable
// object such as a function, method, lambda, or even another callback, allowing
// the runnable object to be invoked later via the callback object.
//
// Unlike function pointers, callbacks are created with base::BindOnce() or
// base::BindRepeating() and support partial function application.
//
// A base::OnceCallback may be Run() at most once; a base::RepeatingCallback may
// be Run() any number of times. |is_null()| is guaranteed to return true for a
// moved-from callback.
//
//   // The lambda takes two arguments, but the first argument |x| is bound at
//   // callback creation.
//   base::OnceCallback<int(int)> cb = base::BindOnce([] (int x, int y) {
//     return x + y;
//   }, 1);
//   // Run() only needs the remaining unbound argument |y|.
//   printf("1 + 2 = %d\n", std::move(cb).Run(2));  // Prints 3
//   printf("cb is null? %s\n",
//          cb.is_null() ? "true" : "false");  // Prints true
//   std::move(cb).Run(2);  // Crashes since |cb| has already run.
//
// Callbacks also support cancellation. A common use is binding the receiver
// object as a WeakPtr<T>. If that weak pointer is invalidated, calling Run()
// will be a no-op. Note that |IsCancelled()| and |is_null()| are distinct:
// simply cancelling a callback will not also make it null.
//
// See //docs/callback.md for the full documentation.

namespace base {

template <typename R, typename... Args>
class OnceCallback<R(Args...)> : public internal::CallbackBase {
 public:
  using ResultType = R;
  using RunType = R(Args...);
  using PolymorphicInvoke = R (*)(internal::BindStateBase*,
                                  internal::PassingType<Args>...);

  constexpr OnceCallback() = default;
  OnceCallback(std::nullptr_t) = delete;

  explicit OnceCallback(internal::BindStateBase* bind_state)
      : internal::CallbackBase(bind_state) {}

  OnceCallback(const OnceCallback&) = delete;
  OnceCallback& operator=(const OnceCallback&) = delete;

  OnceCallback(OnceCallback&&) noexcept = default;
  OnceCallback& operator=(OnceCallback&&) noexcept = default;

  OnceCallback(RepeatingCallback<RunType> other)
      : internal::CallbackBase(std::move(other)) {}

  OnceCallback& operator=(RepeatingCallback<RunType> other) {
    static_cast<internal::CallbackBase&>(*this) = std::move(other);
    return *this;
  }

  R Run(Args... args) const & {
    static_assert(!sizeof(*this),
                  "OnceCallback::Run() may only be invoked on a non-const "
                  "rvalue, i.e. std::move(callback).Run().");
    NOTREACHED();
  }

  R Run(Args... args) && {
    // Move the callback instance into a local variable before the invocation,
    // that ensures the internal state is cleared after the invocation.
    // It's not safe to touch |this| after the invocation, since running the
    // bound function may destroy |this|.
    OnceCallback cb = std::move(*this);
    PolymorphicInvoke f =
        reinterpret_cast<PolymorphicInvoke>(cb.polymorphic_invoke());
    return f(cb.bind_state_.get(), std::forward<Args>(args)...);
  }

  // Then() returns a new OnceCallback that receives the same arguments as
  // |this|, and with the return type of |then|. The returned callback will:
  // 1) Run the functor currently bound to |this| callback.
  // 2) Run the |then| callback with the result from step 1 as its single
  //    argument.
  // 3) Return the value from running the |then| callback.
  //
  // Since this method generates a callback that is a replacement for `this`,
  // `this` will be consumed and reset to a null callback to ensure the
  // originally-bound functor can be run at most once.
  template <typename ThenR, typename... ThenArgs>
  OnceCallback<ThenR(Args...)> Then(OnceCallback<ThenR(ThenArgs...)> then) && {
    CHECK(then);
    return BindOnce(
        internal::ThenHelper<
            OnceCallback, OnceCallback<ThenR(ThenArgs...)>>::CreateTrampoline(),
        std::move(*this), std::move(then));
  }

  // This overload is required; even though RepeatingCallback is implicitly
  // convertible to OnceCallback, that conversion will not used when matching
  // for template argument deduction.
  template <typename ThenR, typename... ThenArgs>
  OnceCallback<ThenR(Args...)> Then(
      RepeatingCallback<ThenR(ThenArgs...)> then) && {
    CHECK(then);
    return BindOnce(
        internal::ThenHelper<
            OnceCallback,
            RepeatingCallback<ThenR(ThenArgs...)>>::CreateTrampoline(),
        std::move(*this), std::move(then));
  }
};

template <typename R, typename... Args>
class RepeatingCallback<R(Args...)> : public internal::CallbackBaseCopyable {
 public:
  using ResultType = R;
  using RunType = R(Args...);
  using PolymorphicInvoke = R (*)(internal::BindStateBase*,
                                  internal::PassingType<Args>...);

  constexpr RepeatingCallback() = default;
  RepeatingCallback(std::nullptr_t) = delete;

  explicit RepeatingCallback(internal::BindStateBase* bind_state)
      : internal::CallbackBaseCopyable(bind_state) {}

  // Copyable and movable.
  RepeatingCallback(const RepeatingCallback&) = default;
  RepeatingCallback& operator=(const RepeatingCallback&) = default;
  RepeatingCallback(RepeatingCallback&&) noexcept = default;
  RepeatingCallback& operator=(RepeatingCallback&&) noexcept = default;

  bool operator==(const RepeatingCallback& other) const {
    return EqualsInternal(other);
  }

  bool operator!=(const RepeatingCallback& other) const {
    return !operator==(other);
  }

  R Run(Args... args) const & {
    PolymorphicInvoke f =
        reinterpret_cast<PolymorphicInvoke>(this->polymorphic_invoke());
    return f(this->bind_state_.get(), std::forward<Args>(args)...);
  }

  R Run(Args... args) && {
    // Move the callback instance into a local variable before the invocation,
    // that ensures the internal state is cleared after the invocation.
    // It's not safe to touch |this| after the invocation, since running the
    // bound function may destroy |this|.
    RepeatingCallback cb = std::move(*this);
    PolymorphicInvoke f =
        reinterpret_cast<PolymorphicInvoke>(cb.polymorphic_invoke());
    return f(std::move(cb).bind_state_.get(), std::forward<Args>(args)...);
  }

  // Then() returns a new RepeatingCallback that receives the same arguments as
  // |this|, and with the return type of |then|. The
  // returned callback will:
  // 1) Run the functor currently bound to |this| callback.
  // 2) Run the |then| callback with the result from step 1 as its single
  //    argument.
  // 3) Return the value from running the |then| callback.
  //
  // If called on an rvalue (e.g. std::move(cb).Then(...)), this method
  // generates a callback that is a replacement for `this`. Therefore, `this`
  // will be consumed and reset to a null callback to ensure the
  // originally-bound functor will be run at most once.
  template <typename ThenR, typename... ThenArgs>
  RepeatingCallback<ThenR(Args...)> Then(
      RepeatingCallback<ThenR(ThenArgs...)> then) const& {
    CHECK(then);
    return BindRepeating(
        internal::ThenHelper<
            RepeatingCallback,
            RepeatingCallback<ThenR(ThenArgs...)>>::CreateTrampoline(),
        *this, std::move(then));
  }

  template <typename ThenR, typename... ThenArgs>
  RepeatingCallback<ThenR(Args...)> Then(
      RepeatingCallback<ThenR(ThenArgs...)> then) && {
    CHECK(then);
    return BindRepeating(
        internal::ThenHelper<
            RepeatingCallback,
            RepeatingCallback<ThenR(ThenArgs...)>>::CreateTrampoline(),
        std::move(*this), std::move(then));
  }
};

}  // namespace base

#endif  // BASE_CALLBACK_H_
