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
#include <utility>

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
// 回调在概念上类似于函数指针：它包装了一个可运行对象，例如函数、方法、lambda，甚至另一个回调，
// 允许稍后通过回调对象调用可运行对象。
//
// Unlike function pointers, callbacks are created with base::BindOnce() or
// base::BindRepeating() and support partial function application.
// 与函数指针不同，回调是使用 base::BindOnce() 或 base::BindRepeating() 创建的，
// 并且支持部分函数应用。
//
// A base::OnceCallback may be Run() at most once; a base::RepeatingCallback may
// be Run() any number of times. |is_null()| is guaranteed to return true for a
// moved-from callback.
// 一个 base::OnceCallback 最多可能是 Run() 一次； base::RepeatingCallback 可以 Run()
// 任意次数。 |is_null()| 对于移出的回调，保证返回 true。
//
//   // The lambda takes two arguments, but the first argument |x| is bound at
//   // callback creation.
//   // lambda 有两个参数，但第一个参数 |x| 在回调创建时绑定。
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
// 回调也支持取消。 一个常见的用途是将接收器对象绑定为 WeakPtr<T>。 如果该弱指针无效，则
// 调用 Run() 将是空操作。 注意 |IsCancelled()| 和 |is_null()| 是不同的：
// 简单地取消回调也不会使其为空。
//
// See //docs/callback.md for the full documentation.

namespace base {

namespace internal {

struct NullCallbackTag {
  template <typename Signature>
  struct WithSignature {};
};

struct DoNothingCallbackTag {
  template <typename Signature>
  struct WithSignature {};
};

}  // namespace internal

template <typename R, typename... Args>
class OnceCallback<R(Args...)> : public internal::CallbackBase {
 public:
  using ResultType = R;
  using RunType = R(Args...);
  using PolymorphicInvoke = R (*)(internal::BindStateBase*,
                                  internal::PassingType<Args>...);

  constexpr OnceCallback() = default;
  OnceCallback(std::nullptr_t) = delete;

  constexpr OnceCallback(internal::NullCallbackTag) : OnceCallback() {}
  constexpr OnceCallback& operator=(internal::NullCallbackTag) {
    *this = OnceCallback();
    return *this;
  }

  constexpr OnceCallback(internal::NullCallbackTag::WithSignature<RunType>)
      : OnceCallback(internal::NullCallbackTag()) {}
  constexpr OnceCallback& operator=(
      internal::NullCallbackTag::WithSignature<RunType>) {
    *this = internal::NullCallbackTag();
    return *this;
  }

  constexpr OnceCallback(internal::DoNothingCallbackTag)
      : OnceCallback(BindOnce([](Args... args) {})) {}
  constexpr OnceCallback& operator=(internal::DoNothingCallbackTag) {
    *this = BindOnce([](Args... args) {});
    return *this;
  }

  constexpr OnceCallback(internal::DoNothingCallbackTag::WithSignature<RunType>)
      : OnceCallback(internal::DoNothingCallbackTag()) {}
  constexpr OnceCallback& operator=(
      internal::DoNothingCallbackTag::WithSignature<RunType>) {
    *this = internal::DoNothingCallbackTag();
    return *this;
  }

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
    PolymorphicInvoke f = reinterpret_cast<PolymorphicInvoke>(cb.polymorphic_invoke());
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
  OnceCallback<ThenR(Args...)> Then(RepeatingCallback<ThenR(ThenArgs...)> then) && {
    CHECK(then);
    return BindOnce(internal::ThenHelper<
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

  constexpr RepeatingCallback(internal::NullCallbackTag)
      : RepeatingCallback() {}
  constexpr RepeatingCallback& operator=(internal::NullCallbackTag) {
    *this = RepeatingCallback();
    return *this;
  }

  constexpr RepeatingCallback(internal::NullCallbackTag::WithSignature<RunType>)
      : RepeatingCallback(internal::NullCallbackTag()) {}
  constexpr RepeatingCallback& operator=(
      internal::NullCallbackTag::WithSignature<RunType>) {
    *this = internal::NullCallbackTag();
    return *this;
  }

  constexpr RepeatingCallback(internal::DoNothingCallbackTag)
      : RepeatingCallback(BindRepeating([](Args... args) {})) {}
  constexpr RepeatingCallback& operator=(internal::DoNothingCallbackTag) {
    *this = BindRepeating([](Args... args) {});
    return *this;
  }

  constexpr RepeatingCallback(
      internal::DoNothingCallbackTag::WithSignature<RunType>)
      : RepeatingCallback(internal::DoNothingCallbackTag()) {}
  constexpr RepeatingCallback& operator=(
      internal::DoNothingCallbackTag::WithSignature<RunType>) {
    *this = internal::DoNothingCallbackTag();
    return *this;
  }

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
