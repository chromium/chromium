// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_OPTIONAL_UTIL_H_
#define BASE_TYPES_OPTIONAL_UTIL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

// Helper for converting an `absl::optional<T>` to a pointer suitable for
// passing as a function argument:
//
// void MaybeProcessData(const std::string* optional_data);
//
// class Example {
//  public:
//   void DoSomething() {
//     MaybeProcessData(base::OptionalToPtr(data_));
//   }
//
//  private:
//   absl::optional<std::string> data_;
// };
//
// Rationale: per the C++ style guide, if `T` would normally be passed by
// reference, the optional version should be passed as `T*`, and *not* as
// `const absl::optional<T>&`. Passing as `const absl::optional<T>&` leads to
// implicit constructions and copies, e.g.:
//
// // BAD: a caller passing a `std::string` implicitly copies the entire string
// // to construct a temporary `absl::optional<std::string>` to use for the
// // function argument.
// void BadMaybeProcessData(const absl::optional<std::string>& optional_data);
//
// For more background, see https://abseil.io/tips/163.
template <class T>
const T* OptionalToPtr(const absl::optional<T>& optional) {
  return optional.has_value() ? &optional.value() : nullptr;
}

template <class T>
T* OptionalToPtr(absl::optional<T>& optional) {
  return optional.has_value() ? &optional.value() : nullptr;
}

// Helper for creating an `absl::optional<T>` from a `T*` which may be null.
template <class T>
absl::optional<T> OptionalFromPtr(const T* value) {
  return value ? absl::optional<T>(*value) : absl::nullopt;
}

}  // namespace base

#endif  // BASE_TYPES_OPTIONAL_UTIL_H_
