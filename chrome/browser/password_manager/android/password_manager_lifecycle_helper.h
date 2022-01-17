// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LIFECYCLE_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LIFECYCLE_HELPER_H_

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"  // callback_forward doesn't suffice for members.

// This helper listens to Android lifecycle events like OnForegroundSessionStart
// and calls a registered callback synchronously if it occurs.
// Lifecycle events are not buffered or repeated — if the helper was added after
// an event happened, it will not be triggered.
class PasswordManagerLifecycleHelper {
 public:
  // The passed `foregrounding_callback` is called synchronously every time
  // chrome starts a foreground session (see `OnForegroundSessionStart` below).
  explicit PasswordManagerLifecycleHelper(
      base::RepeatingClosure foregrounding_callback);
  ~PasswordManagerLifecycleHelper();
  PasswordManagerLifecycleHelper(PasswordManagerLifecycleHelper&&) = delete;
  PasswordManagerLifecycleHelper(const PasswordManagerLifecycleHelper&) =
      delete;
  PasswordManagerLifecycleHelper& operator=(PasswordManagerLifecycleHelper&&) =
      delete;
  PasswordManagerLifecycleHelper& operator=(
      const PasswordManagerLifecycleHelper&) = delete;

  // Called via JNI. Called when chrome starts a top-level activity if none
  // has been in the foreground yet. Check the java implementation at
  // ChromeActivitySessionTrack#onForegroundSessionStart for more details.
  void OnForegroundSessionStart(JNIEnv* env);

 private:
  base::RepeatingClosure foregrounding_callback_;

  // Reference to the singleton instance of the Java counterpart of this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LIFECYCLE_HELPER_H_
