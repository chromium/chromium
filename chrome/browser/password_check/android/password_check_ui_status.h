// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_UI_STATUS_H_
#define CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_UI_STATUS_H_

namespace password_manager {

// Enumerates the possible states of the password check, in a way that can be
// used to display information in the password check header.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_check
enum class PasswordCheckUIStatus {
  kIdle,
  kRunning,
  kCanceled,
  kErrorOffline,
  kErrorNoPasswords,
  kErrorSignedOut,
  kErrorQuotaLimit,
  kErrorQuotaLimitAccountCheck,
  kErrorUnknown,
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_UI_STATUS_H_
