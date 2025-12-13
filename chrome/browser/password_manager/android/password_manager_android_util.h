// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_

#include <memory>

#include "chrome/browser/password_manager/android/password_manager_util_bridge_interface.h"

namespace password_manager_android_util {

// Checks whether the password manager can be used on Android.
// The criteria are:
// - access to the internal backend
// - GMS Core version with full UPM support
bool IsPasswordManagerAvailable(
    std::unique_ptr<PasswordManagerUtilBridgeInterface> util_bridge);

// As above, except the caller already knows whether the internal backend
// is present, probably because the call originates in Java.
bool IsPasswordManagerAvailable(bool is_internal_backend_present);

}  // namespace password_manager_android_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_
