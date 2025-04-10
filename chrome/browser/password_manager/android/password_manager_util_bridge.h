// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UTIL_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UTIL_BRIDGE_H_

#include "chrome/browser/password_manager/android/password_manager_util_bridge_interface.h"

namespace password_manager_android_util {

// Util bridge allowing C++ to check information that is only available
// via Java.
class PasswordManagerUtilBridge : public PasswordManagerUtilBridgeInterface {
 public:
  PasswordManagerUtilBridge() = default;
  ~PasswordManagerUtilBridge() override = default;
  PasswordManagerUtilBridge(const PasswordManagerUtilBridge&) = delete;
  PasswordManagerUtilBridge& operator=(const PasswordManagerUtilBridge&) =
      delete;

  bool IsInternalBackendPresent() override;
  bool IsPlayStoreAppPresent() override;
  bool IsGooglePlayServicesUpdatable() override;
};

}  // namespace password_manager_android_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UTIL_BRIDGE_H_
