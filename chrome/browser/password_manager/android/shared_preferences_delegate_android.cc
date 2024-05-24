// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/shared_preferences_delegate_android.h"

#include "base/android/shared_preferences/shared_preferences_manager.h"
#include "chrome/browser/preferences/android/chrome_shared_preferences.h"

namespace {
//  Password Protection key for SharedPreferencesManager.
//  Must be kept in sync with ChromePreferenceKeys.java.
constexpr char kPasswordProtectionAccounts[] =
    "Chrome.PasswordProtection.Accounts";
}  // namespace

SharedPreferencesDelegateAndroid::SharedPreferencesDelegateAndroid() = default;

SharedPreferencesDelegateAndroid::~SharedPreferencesDelegateAndroid() = default;

std::string SharedPreferencesDelegateAndroid::GetCredentials(
    const std::string& default_value) {
  base::android::SharedPreferencesManager shared_prefs =
      android::shared_preferences::GetChromeSharedPreferences();
  return shared_prefs.ReadString(kPasswordProtectionAccounts, default_value);
}

void SharedPreferencesDelegateAndroid::SetCredentials(
    const std::string& default_value) {
  base::android::SharedPreferencesManager shared_prefs =
      android::shared_preferences::GetChromeSharedPreferences();
  return shared_prefs.WriteString(kPasswordProtectionAccounts, default_value);
}
