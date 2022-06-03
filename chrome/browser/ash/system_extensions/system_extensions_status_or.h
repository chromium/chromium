// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_

#include "chrome/browser/ash/system_extensions/system_extension.h"

// SystemExtensionsStatusOr is a union of an status enum class and an object.
// This class either holds an object in a usable state, or a status code
// explaining why `T` is not present. This class is typically the return value
// of a function which may fail.
//
// An SystemExtensionsStatusOr can never hold an "OK" status (an `S::kOk`
// value); instead, the presence of `T` indicates success. Instead of checking
// for a `kOk` value, use the `ok()` member function.
//
// There is nothing SystemExtensions specific about this class so if needed
// this can be moved to //base.
template <typename T, typename S>
class SystemExtensionsStatusOr {
 public:
  // Constructs a new `SystemExtensionsStatusOr` with an `S::kUnknown` status.
  // This constructor is marked 'explicit' to prevent usages in return values
  // such as 'return {};'.
  explicit SystemExtensionsStatusOr() : status_(S::kUnknown) {}  // NOLINT

  // All of these are implicit, so that one may just return Status or
  // SystemExtension.
  SystemExtensionsStatusOr(S status) : status_(status) {}  // NOLINT
  SystemExtensionsStatusOr(T value)                        // NOLINT
      : status_(S::kOk), value_(std::move(value)) {}

  SystemExtensionsStatusOr(SystemExtensionsStatusOr&&) = default;
  SystemExtensionsStatusOr& operator=(SystemExtensionsStatusOr&&) = default;

  ~SystemExtensionsStatusOr() = default;

  bool ok() const { return status_ == S::kOk; }

  // Returns the status code when the status is not kOk. Crashes if the
  // status is kOk.
  S status() {
    CHECK(!ok());
    return status_;
  }

  // Returns the object if ok() is true. CHECKs otherwise.
  const T& value() const& {
    CHECK(ok());
    return value_;
  }

  // Returns the SystemExtension if ok() is true. CHECKs otherwise.
  T&& value() && {
    CHECK(ok());
    return std::move(value_);
  }

 private:
  S status_;
  T value_;
};

template <typename S>
using StatusOrSystemExtension = SystemExtensionsStatusOr<SystemExtension, S>;

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_
