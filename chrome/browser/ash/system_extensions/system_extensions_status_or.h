// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_

#include "base/types/expected.h"

namespace ash {

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
      : value_or_status_(base::unexpect, S::kUnknown) {}

  // All of these are implicit, so that one may just return `S` or `T`.
  SystemExtensionsStatusOr(S status)  // NOLINT
      : value_or_status_(base::unexpect, status) {}
  SystemExtensionsStatusOr(T value)  // NOLINT
      : value_or_status_(std::move(value)) {}

  SystemExtensionsStatusOr(SystemExtensionsStatusOr&&) = default;
  SystemExtensionsStatusOr& operator=(SystemExtensionsStatusOr&&) = default;

  ~SystemExtensionsStatusOr() = default;

  bool ok() const { return value_or_status_.has_value(); }

  // Returns the status code when the status is not kOk. Crashes if the
  // status is kOk.
  S status() {
    CHECK(!ok());
    return value_or_status_.error();
  }

  // Returns `T` if ok() is true. CHECKs otherwise.
  const T& value() const& {
    CHECK(ok());
    return value_or_status_.value();
  }

  // Returns the `T` if ok() is true. CHECKs otherwise.
  T&& value() && {
    CHECK(ok());
    return std::move(value_or_status_.value());
  }

 private:
  base::expected<T, S> value_or_status_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_
