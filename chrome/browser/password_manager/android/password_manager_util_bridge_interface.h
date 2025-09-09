// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UTIL_BRIDGE_INTERFACE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UTIL_BRIDGE_INTERFACE_H_

namespace password_manager_android_util {

// Interface for a password manager util bridge between C++ and Java.
// It can be mocked in tests to avoid JNI calls and ensure deterministic
// results, as some of the checks depend on bot and build configuration.
class PasswordManagerUtilBridgeInterface {
 public:
  PasswordManagerUtilBridgeInterface() = default;
  virtual ~PasswordManagerUtilBridgeInterface() = default;

  // Returns whether Chrome's internal backend is available.
  virtual bool IsInternalBackendPresent() = 0;

  // Returns whether Google Play Services is installed on the device and
  // whether Google Play Store exists to allow updating Google Play Services.
  virtual bool IsGooglePlayServicesUpdatable() = 0;
};

}  // namespace password_manager_android_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UTIL_BRIDGE_INTERFACE_H_
