// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SHARED_PREFERENCES_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SHARED_PREFERENCES_DELEGATE_ANDROID_H_

#include "components/password_manager/core/browser/shared_preferences_delegate.h"

// This enables access to Android SharedPreferences.
class SharedPreferencesDelegateAndroid
    : public password_manager::SharedPreferencesDelegate {
 public:
  SharedPreferencesDelegateAndroid();
  ~SharedPreferencesDelegateAndroid() override;
  SharedPreferencesDelegateAndroid(const SharedPreferencesDelegateAndroid&) =
      delete;
  SharedPreferencesDelegateAndroid& operator=(
      const SharedPreferencesDelegateAndroid&) = delete;
  std::string GetCredentials(const std::string& default_value) override;
  void SetCredentials(const std::string& value) override;
};
#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SHARED_PREFERENCES_DELEGATE_ANDROID_H_
