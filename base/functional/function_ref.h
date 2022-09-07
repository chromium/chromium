// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUNCTIONAL_FUNCTION_REF_H_
#define BASE_FUNCTIONAL_FUNCTION_REF_H_

#include <type_traits>
#include <utility>

#include "base/bind_internal.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "third_party/abseil-cpp/absl/functional/function_ref.h"

namespace base {

template <typename Signature>
class FunctionRef;

// A non-owning reference to any invocable object (e.g. function pointer, method
// pointer, functor, lambda, et cetera) suitable for use as a type-erased
// argument to ForEach-style functions or other visitor patterns that:
//
// - do not need to copy or take ownership of the argument
// - synchronously call the invocable that was passed as an argument
//
// `base::FunctionRef` makes no heap allocations: it is trivially copyable and
// should be passed by value.
//
// `base::FunctionRef` has no null/empty state: a `base::FunctionRef` is always
// valid to invoke.
//
// The usual lifetime precautions for other non-owning references types (e.g.
// `base::StringPiece`, `base::span`) also apply to `base::FunctionRef`.
// `base::FunctionRef` should typically be used as an argument; returning a
// `base::FunctionRef` or storing a `base::FunctionRef` as a field is dangerous
// and likely to result in lifetime bugs.
//
// `base::RepeatingCallback` and `base::BindRepeating()` is another common way
// to represent type-erased invocable objects. In contrast, it requires a heap
// allocation and is not trivially copyable. It should be used when there are
// ownership requirements (e.g. partial application of arguments to a function
// stored for asynchronous execution).
//
// Note: this class is very similar to `absl::FunctionRef<R(Args...)>`, but
// disallows implicit conversions between function types, e.g. functors that
// return non-void values may bind to `absl::FunctionRef<void(...)>`:
//
// ```
// void F(absl::FunctionRef<void()>);
// ...
//   F([] { return 42; });
// ```
//
// This compiles and silently discards the return value, but with the base
// version, the equivalent snippet:
//
// ```
// void F(base::FunctionRef<void()>);
// ...
//   F([] { return 42;});
// ```
//
// will not compile at all.
template <typename R, typename... Args>
class FunctionRef<R(Args...)> {
 public:
  // `ABSL_ATTRIBUTE_LIFETIME_BOUND` is important since `FunctionRef` retains
  // only a reference to `functor`, `functor` must outlive `this`.
  template <typename Functor,
            typename = std::enable_if_t<std::is_same_v<
                R(Args...),
                typename internal::MakeFunctorTraits<Functor>::RunType>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  FunctionRef(const Functor& functor ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : wrapped_func_ref_(functor) {}

  // Null FunctionRefs are not allowed.
  FunctionRef() = delete;

  FunctionRef(const FunctionRef&) = default;
  // Reduce the likelihood of lifetime bugs by disallowing assignment.
  FunctionRef& operator=(const FunctionRef&) = delete;

  R operator()(Args... args) const {
    return wrapped_func_ref_(std::forward<Args>(args)...);
  }

  absl::FunctionRef<R(Args...)> ToAbsl() const { return wrapped_func_ref_; }

  // In Chrome, converting to `absl::FunctionRef` should be explicitly done
  // through `ToAbsl()`.
  template <typename Signature>
  operator absl::FunctionRef<Signature>() = delete;

 private:
  absl::FunctionRef<R(Args...)> wrapped_func_ref_;
};

}  // namespace base

#endif  // BASE_FUNCTIONAL_FUNCTION_REF_H_
