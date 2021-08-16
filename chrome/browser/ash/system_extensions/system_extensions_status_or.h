// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_

#include "chrome/browser/ash/system_extensions/system_extension.h"

// StatusOrSystemExtension is a union of an status enum class and a
// SystemExtension. This class either holds an object in a usable state, or a
// status code explaining why SystemExtension is not present. This class is
// typically the return value of a function which may fail.
//
// An StatusOrSystemExtension can never hold an "OK" status (an `S::kOk` value);
// instead, the presence of SystemExtension indicates success. Instead of
// checking for a `kOk` value, use the `ok()` member function.
template <typename S>
class StatusOrSystemExtension {
 public:
  // Constructs a new `StatusOrSystemExtension` with an `S::kUnknown` status.
  // This constructor is marked 'explicit' to prevent usages in return values
  // such as 'return {};'.
  explicit StatusOrSystemExtension() : status_(S::kUnknown) {}  // NOLINT

  // All of these are implicit, so that one may just return Status or
  // SystemExtension.
  StatusOrSystemExtension(S status) : status_(status) {}     // NOLINT
  StatusOrSystemExtension(SystemExtension system_extension)  // NOLINT
      : status_(S::kOk), system_extension_(std::move(system_extension)) {}

  StatusOrSystemExtension(StatusOrSystemExtension&&) = default;
  StatusOrSystemExtension& operator=(StatusOrSystemExtension&&) = default;

  ~StatusOrSystemExtension() = default;

  bool ok() const { return status_ == S::kOk; }

  // Returns the status code when the status is not kOk. Crashes if the
  // status is kOk.
  S status() {
    CHECK(!ok());
    return status_;
  }

  // Returns the SystemExtension if ok() is true. CHECKs otherwise.
  const SystemExtension& system_extension() const& {
    CHECK(ok());
    return system_extension_;
  }

  // Returns the SystemExtension if ok() is true. CHECKs otherwise.
  SystemExtension&& system_extension() && {
    CHECK(ok());
    return std::move(system_extension_);
  }

 private:
  S status_;
  SystemExtension system_extension_;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_STATUS_OR_H_
