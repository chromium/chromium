// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LIFECYCLE_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LIFECYCLE_HELPER_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"

// The interface is implemented by JNI helper classes that forward Android
// lifecycle events for use in the password manager.
class PasswordManagerLifecycleHelper {
 public:
  PasswordManagerLifecycleHelper() = default;

  PasswordManagerLifecycleHelper(PasswordManagerLifecycleHelper&&) = delete;
  PasswordManagerLifecycleHelper(const PasswordManagerLifecycleHelper&) =
      delete;
  PasswordManagerLifecycleHelper& operator=(PasswordManagerLifecycleHelper&&) =
      delete;
  PasswordManagerLifecycleHelper& operator=(
      const PasswordManagerLifecycleHelper&) = delete;

  // The passed `foregrounding_callback` is expected to be called synchronously
  // every time chrome starts a foreground session.
  virtual void RegisterObserver(
      base::RepeatingClosure foregrounding_callback) = 0;

  // Expects subclasses to stop listening for lifecycle events.
  virtual void UnregisterObserver() = 0;

  virtual ~PasswordManagerLifecycleHelper() = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LIFECYCLE_HELPER_H_
