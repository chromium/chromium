// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LIFECYCLE_HELPER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LIFECYCLE_HELPER_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"  // callback_forward doesn't suffice for members.
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"

// Simple JNI bridge to implement the PasswordManagerLifecycleHelper interface.
// This helper listens to Android lifecycle events like OnForegroundSessionStart
// and calls a registered callback synchronously if it occurs.
// Lifecycle events are not buffered or repeated — if the helper was added after
// an event happened, it will not be triggered.
class PasswordManagerLifecycleHelperImpl
    : public PasswordManagerLifecycleHelper {
 public:
  PasswordManagerLifecycleHelperImpl();

  // PasswordManagerLifecycleHelper:
  void RegisterObserver(base::RepeatingClosure foregrounding_callback) override;
  void UnregisterObserver() override;
  ~PasswordManagerLifecycleHelperImpl() override;

  // Called via JNI. Called when chrome starts a top-level activity if none
  // has been in the foreground yet. Check the java implementation at
  // ChromeActivitySessionTrack#onForegroundSessionStart for more details.
  void OnForegroundSessionStart(JNIEnv* env);

 private:
  base::RepeatingClosure foregrounding_callback_;

  // Reference to the singleton instance of the Java counterpart of this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LIFECYCLE_HELPER_IMPL_H_
