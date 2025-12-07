// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"

#if CHECK_WILL_STREAM()
#include "base/logging.h"
#endif  // CHECK_WILL_STREAM()

#ifndef BASE_CHECK_DEREF_H_
#define BASE_CHECK_DEREF_H_

namespace logging {

// Dereferences the pointer-like object `val` if `val` doesn't test `false`, or
// dies if it does.
//
// It is useful in initializers and direct assignments, where a direct `CHECK`
// call can't be used:
//
//   MyType& type_ref = CHECK_DEREF(MethodReturningAPointerLikeObject());
//
#define CHECK_DEREF(ptr) ::logging::CheckDeref(ptr, "*" #ptr)

template <typename T>
[[nodiscard]] T& CheckDeref(
    T* ptr,
    const char* message,
    const base::Location& location = base::Location::Current()) {
  // Note: we can't just call `CHECK_NE(ptr, nullptr)` here, as that would
  // cause the error to be reported from this header, and we want the error
  // to be reported at the file and line of the caller.
  if (ptr == nullptr) [[unlikely]] {
#if CHECK_WILL_STREAM()
    // `CheckNoreturnError` will die with a fatal error in its destructor.
    CheckNoreturnError::Check(message, location);
#else
    CheckFailure();
#endif  // !CHECK_WILL_STREAM()
  }
  return *ptr;
}

template <typename T>
[[nodiscard]] decltype(auto) CheckDeref(
    T&& val LIFETIME_BOUND,
    const char* message,
    const base::Location& location = base::Location::Current()) {
  // Note: we can't just call `CHECK(val)` here, as that would cause the error
  // to be reported from this header, and we want the error to be reported at
  // the file and line of the caller.
  if (!val) [[unlikely]] {
#if CHECK_WILL_STREAM()
    // `CheckNoreturnError` will die with a fatal error in its destructor.
    CheckNoreturnError::Check(message, location);
#else
    CheckFailure();
#endif  // !CHECK_WILL_STREAM()
  }
  return *std::forward<T>(val);
}

}  // namespace logging

#endif  // BASE_CHECK_DEREF_H_
