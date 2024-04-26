// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UTIL_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UTIL_BRIDGE_H_

namespace password_manager_android_util {
// Returns whether Chrome's internal backend is available.
bool IsInternalBackendPresent();

// Returns whether Play Store is installed on the device.
bool IsPlayStoreAppPresent();
}  // namespace password_manager_android_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UTIL_BRIDGE_H_
