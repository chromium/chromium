// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines helpful methods for dealing with Callbacks.  Because Callbacks
// are implemented using templates, with a class per callback signature, adding
// methods to Callback<> itself is unattractive (lots of extra code gets
// generated).  Instead, consider adding methods here.

#ifndef BASE_CALLBACK_HELPERS_H_
#define BASE_CALLBACK_HELPERS_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"

namespace base {

namespace internal {

template <typename T>
struct IsBaseCallbackImpl : std::false_type {};

template <typename R, typename... Args>
struct IsBaseCallbackImpl<OnceCallback<R(Args...)>> : std::true_type {};

template <typename R, typename... Args>
struct IsBaseCallbackImpl<RepeatingCallback<R(Args...)>> : std::true_type {};

template <typename T>
struct IsOnceCallbackImpl : std::false_type {};

template <typename R, typename... Args>
struct IsOnceCallbackImpl<OnceCallback<R(Args...)>> : std::true_type {};

}  // namespace internal

// IsBaseCallback<T>::value is true when T is any of the Closure or Callback
// family of types.
template <typename T>
using IsBaseCallback = internal::IsBaseCallbackImpl<std::decay_t<T>>;

// IsOnceCallback<T>::value is true when T is a OnceClosure or OnceCallback
// type.
template <typename T>
using IsOnceCallback = internal::IsOnceCallbackImpl<std::decay_t<T>>;

// SFINAE friendly enabler allowing to overload methods for both Repeating and
// OnceCallbacks.
//
// Usage:
// template <template <typename> class CallbackType,
//           ... other template args ...,
//           typename = EnableIfIsBaseCallback<CallbackType>>
// void DoStuff(CallbackType<...> cb, ...);
template <template <typename> class CallbackType>
using EnableIfIsBaseCallback =
    std::enable_if_t<IsBaseCallback<CallbackType<void()>>::value>;

namespace internal {

template <typename... Args>
class OnceCallbackHolder final {
 public:
  OnceCallbackHolder(OnceCallback<void(Args...)> callback,
                     bool ignore_extra_runs)
      : callback_(std::move(callback)), ignore_extra_runs_(ignore_extra_runs) {
    DCHECK(callback_);
  }
  OnceCallbackHolder(const OnceCallbackHolder&) = delete;
  OnceCallbackHolder& operator=(const OnceCallbackHolder&) = delete;

  void Run(Args... args) {
    if (subtle::NoBarrier_AtomicExchange(&has_run_, 1)) {
      CHECK(ignore_extra_runs_) << "Both OnceCallbacks returned by "
                                   "base::SplitOnceCallback() were run. "
                                   "At most one of the pair should be run.";
      return;
    }
    DCHECK(callback_);
    std::move(callback_).Run(std::forward<Args>(args)...);
  }

 private:
  volatile subtle::Atomic32 has_run_ = 0;
  base::OnceCallback<void(Args...)> callback_;
  const bool ignore_extra_runs_;
};

}  // namespace internal

// Wraps the given OnceCallback and returns two OnceCallbacks with an identical
// signature. On first invokation of either returned callbacks, the original
// callback is invoked. Invoking the remaining callback results in a crash.
template <typename... Args>
std::pair<OnceCallback<void(Args...)>, OnceCallback<void(Args...)>>
SplitOnceCallback(OnceCallback<void(Args...)> callback) {
  if (!callback) {
    // Empty input begets two empty outputs.
    return std::make_pair(OnceCallback<void(Args...)>(),
                          OnceCallback<void(Args...)>());
  }
  using Helper = internal::OnceCallbackHolder<Args...>;
  auto wrapped_once = base::BindRepeating(
      &Helper::Run, std::make_unique<Helper>(std::move(callback),
                                             /*ignore_extra_runs=*/false));
  return std::make_pair(wrapped_once, wrapped_once);
}

// ScopedClosureRunner is akin to std::unique_ptr<> for Closures. It ensures
// that the Closure is executed no matter how the current scope exits.
// If you are looking for "ScopedCallback", "CallbackRunner", or
// "CallbackScoper" this is the class you want.
class BASE_EXPORT ScopedClosureRunner {
 public:
  ScopedClosureRunner();
  explicit ScopedClosureRunner(OnceClosure closure);
  ScopedClosureRunner(ScopedClosureRunner&& other);
  // Runs the current closure if it's set, then replaces it with the closure
  // from |other|. This is akin to how unique_ptr frees the contained pointer in
  // its move assignment operator. If you need to explicitly avoid running any
  // current closure, use ReplaceClosure().
  ScopedClosureRunner& operator=(ScopedClosureRunner&& other);
  ~ScopedClosureRunner();

  explicit operator bool() const { return !!closure_; }

  // Calls the current closure and resets it, so it wont be called again.
  void RunAndReset();

  // Replaces closure with the new one releasing the old one without calling it.
  void ReplaceClosure(OnceClosure closure);

  // Releases the Closure without calling.
  OnceClosure Release() WARN_UNUSED_RESULT;

 private:
  OnceClosure closure_;
};

// Creates a null callback.
class BASE_EXPORT NullCallback {
 public:
  template <typename R, typename... Args>
  operator RepeatingCallback<R(Args...)>() const {
    return RepeatingCallback<R(Args...)>();
  }
  template <typename R, typename... Args>
  operator OnceCallback<R(Args...)>() const {
    return OnceCallback<R(Args...)>();
  }
};

// Creates a callback that does nothing when called.
class BASE_EXPORT DoNothing {
 public:
  template <typename... Args>
  operator RepeatingCallback<void(Args...)>() const {
    return Repeatedly<Args...>();
  }
  template <typename... Args>
  operator OnceCallback<void(Args...)>() const {
    return Once<Args...>();
  }
  // Explicit way of specifying a specific callback type when the compiler can't
  // deduce it.
  template <typename... Args>
  static RepeatingCallback<void(Args...)> Repeatedly() {
    return BindRepeating([](Args... args) {});
  }
  template <typename... Args>
  static OnceCallback<void(Args...)> Once() {
    return BindOnce([](Args... args) {});
  }
};

// Useful for creating a Closure that will delete a pointer when invoked. Only
// use this when necessary. In most cases MessageLoop::DeleteSoon() is a better
// fit.
template <typename T>
void DeletePointer(T* obj) {
  delete obj;
}

}  // namespace base

#endif  // BASE_CALLBACK_HELPERS_H_
