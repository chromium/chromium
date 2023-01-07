// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_FAKE_PASSWORD_MANAGER_LIFECYCLE_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_FAKE_PASSWORD_MANAGER_LIFECYCLE_HELPER_H_

#include "base/functional/callback.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"

namespace password_manager {

class FakePasswordManagerLifecycleHelper
    : public PasswordManagerLifecycleHelper {
 public:
  FakePasswordManagerLifecycleHelper();
  ~FakePasswordManagerLifecycleHelper() override;

  // PasswordManagerLifecycleHelper implementation
  void RegisterObserver(base::RepeatingClosure foregrounding_callback) override;
  void UnregisterObserver() override;

  void OnForegroundSessionStart();

 private:
  base::RepeatingClosure foregrounding_callback_;
};

}  // namespace password_manager
#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_FAKE_PASSWORD_MANAGER_LIFECYCLE_HELPER_H_
