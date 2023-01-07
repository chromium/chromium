// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_OPERATION_TARGET_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_OPERATION_TARGET_H_

namespace password_manager {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
enum class PasswordStoreOperationTarget {
  // Uses default storage depending on a sync status at the moment of a call.
  kDefault = 0,
  // Forces to use Syncing storage for the latest sync account neglecting
  // current sync status.
  kSyncingStorage = 1,
  // Forces to use Local storage neglecting current sync status.
  kLocalStorage = 2,
  kMaxValue = kLocalStorage,
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_OPERATION_TARGET_H_
