// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines helpful methods for dealing with Callbacks.  Because Callbacks
// are implemented using templates, with a class per callback signature, adding
// methods to Callback<> itself is unattractive (lots of extra code gets
// generated).  Instead, consider adding methods here.

#ifndef BASE_FUNCTIONAL_CALLBACK_HELPERS_H_
#define BASE_FUNCTIONAL_CALLBACK_HELPERS_H_

#include <atomic>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_tags.h"

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
    if (has_run_.exchange(true, std::memory_order_relaxed)) {
      CHECK(ignore_extra_runs_) << "Both OnceCallbacks returned by "
                                   "base::SplitOnceCallback() were run. "
                                   "At most one of the pair should be run.";
      return;
    }
    DCHECK(callback_);
    std::move(callback_).Run(std::forward<Args>(args)...);
  }

 private:
  std::atomic<bool> has_run_{false};
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

// Convenience helper to allow a `closure` to be used in a context which is
// expecting a callback with arguments. Returns a null callback if `closure` is
// null.
template <typename... Args>
RepeatingCallback<void(Args...)> IgnoreArgs(RepeatingClosure closure) {
  return closure ? BindRepeating([](Args...) {}).Then(std::move(closure))
                 : RepeatingCallback<void(Args...)>();
}

// Convenience helper to allow a `closure` to be used in a context which is
// expecting a callback with arguments. Returns a null callback if `closure` is
// null.
template <typename... Args>
OnceCallback<void(Args...)> IgnoreArgs(OnceClosure closure) {
  return closure ? BindOnce([](Args...) {}).Then(std::move(closure))
                 : OnceCallback<void(Args...)>();
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
  [[nodiscard]] OnceClosure Release();

 private:
  OnceClosure closure_;
};

// Returns a placeholder type that will implicitly convert into a null callback,
// similar to how absl::nullopt / std::nullptr work in conjunction with
// absl::optional and various smart pointer types.
constexpr auto NullCallback() {
  return internal::NullCallbackTag();
}

// Returns a placeholder type that will implicitly convert into a callback that
// does nothing, similar to how absl::nullopt / std::nullptr work in conjunction
// with absl::optional and various smart pointer types.
constexpr auto DoNothing() {
  return internal::DoNothingCallbackTag();
}

// Similar to the above, but with a type hint. Useful for disambiguating
// among multiple function overloads that take callbacks with different
// signatures:
//
// void F(base::OnceCallback<void()> callback);     // 1
// void F(base::OnceCallback<void(int)> callback);  // 2
//
// F(base::NullCallbackAs<void()>());               // calls 1
// F(base::DoNothingAs<void(int)>());               // calls 2
template <typename Signature>
constexpr auto NullCallbackAs() {
  return internal::NullCallbackTag::WithSignature<Signature>();
}

template <typename Signature>
constexpr auto DoNothingAs() {
  return internal::DoNothingCallbackTag::WithSignature<Signature>();
}

// Similar to DoNothing above, but with bound arguments. This helper is useful
// for keeping objects alive until the callback runs.
// Example:
//
// void F(base::OnceCallback<void(int)> result_callback);
//
// std::unique_ptr<MyClass> ptr;
// F(base::DoNothingWithBoundArgs(std::move(ptr)));
template <typename... Args>
constexpr auto DoNothingWithBoundArgs(Args&&... args) {
  return internal::DoNothingCallbackTag::WithBoundArguments(
      std::forward<Args>(args)...);
}

// Useful for creating a Closure that will delete a pointer when invoked. Only
// use this when necessary. In most cases MessageLoop::DeleteSoon() is a better
// fit.
template <typename T>
void DeletePointer(T* obj) {
  delete obj;
}

}  // namespace base

#endif  // BASE_FUNCTIONAL_CALLBACK_HELPERS_H_
