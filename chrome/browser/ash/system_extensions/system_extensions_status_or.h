// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_

#include "third_party/abseil-cpp/absl/types/variant.h"

// SystemExtensionsStatusOr is a union of an status enum class and an object.
// This class either holds an object in a usable state, or a status code
// explaining why `T` is not present. This class is typically the return value
// of a function which may fail.
//
// An SystemExtensionsStatusOr can never hold an "OK" status, instead, the
// presence of `T` indicates success. Instead of checking for a `kOk` value, use
// the `ok()` member function.
//
// There is nothing SystemExtensions specific about this class so if needed
// this can be moved to //base.
template <typename S, typename T>
class SystemExtensionsStatusOr {
 public:
  // Constructs a new `SystemExtensionsStatusOr` with an `S::kUnknown` status.
  // This constructor is marked 'explicit' to prevent usages in return values
  // such as 'return {};'.
  explicit SystemExtensionsStatusOr()  // NOLINT
      : status_or_value_(S::kUnknown) {}

  // All of these are implicit, so that one may just return `S` or `T`.
  SystemExtensionsStatusOr(S status) : status_or_value_(status) {}  // NOLINT
  SystemExtensionsStatusOr(T value)                                 // NOLINT
      : status_or_value_(std::move(value)) {}

  SystemExtensionsStatusOr(SystemExtensionsStatusOr&&) = default;
  SystemExtensionsStatusOr& operator=(SystemExtensionsStatusOr&&) = default;

  ~SystemExtensionsStatusOr() = default;

  bool ok() const { return absl::holds_alternative<T>(status_or_value_); }

  // Returns the status code when the status is not kOk. Crashes if the
  // status is kOk.
  S status() {
    CHECK(!ok());
    return absl::get<S>(status_or_value_);
  }

  // Returns `T` if ok() is true. CHECKs otherwise.
  const T& value() const& {
    CHECK(ok());
    return absl::get<T>(status_or_value_);
  }

  // Returns the `T` if ok() is true. CHECKs otherwise.
  T&& value() && {
    CHECK(ok());
    return std::move(absl::get<T>(status_or_value_));
  }

 private:
  absl::variant<S, T> status_or_value_;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_
