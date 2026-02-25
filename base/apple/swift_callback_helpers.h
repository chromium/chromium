// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_APPLE_SWIFT_CALLBACK_HELPERS_H_
#define BASE_APPLE_SWIFT_CALLBACK_HELPERS_H_

#include "base/check.h"
#include "base/functional/callback.h"

namespace base {
namespace swift_helpers {

// Converts a base::RepeatingCallback into a standard C++ std::function.
// Since RepeatingCallback can be copied and run multiple times, the returned
// std::function can also be invoked multiple times.
// This helper function is specifically designed for passing callbacks across
// the C++/Swift boundary when C++/Swift interop is enabled.
template <typename ReturnVal, typename... Args>
std::function<ReturnVal(Args...)> ToStdFunction(
    base::RepeatingCallback<ReturnVal(Args...)> cb) {
  return [cb = std::move(cb)](Args... args) -> ReturnVal {
    return cb.Run(std::forward<Args>(args)...);
  };
}

// Converts a base::OnceCallback into a standard C++ std::function.
// While the returned `std::function` is technically copyable and callable
// multiple times by C++ standards, the underlying `base::OnceCallback` can only
// be executed once. Invoking the returned `std::function` more than once will
// trigger a CHECK crash.
// This helper function is specifically designed for passing callbacks across
// the C++/Swift boundary when C++/Swift interop is enabled.
template <typename ReturnVal, typename... Args>
std::function<ReturnVal(Args...)> ToStdFunction(
    base::OnceCallback<ReturnVal(Args...)> cb) {
  using CallbackType = base::OnceCallback<ReturnVal(Args...)>;
  // std::function requires its target to be copyable.
  // By wrapping the base::OnceCallback in a std::shared_ptr, the lambda
  // captures the pointer instead of the callback itself.
  auto shared_cb = std::make_shared<CallbackType>(std::move(cb));

  // The mutable keyword allows to move a value.
  return [shared_cb](Args... args) mutable -> ReturnVal {
    CHECK(shared_cb && !shared_cb->is_null())
        << "OnceCallback invoked more than once from Swift.";
    return std::move(*shared_cb).Run(std::forward<Args>(args)...);
  };
}

}  // namespace swift_helpers
}  // namespace base

#endif  // BASE_APPLE_SWIFT_CALLBACK_HELPERS_H_
