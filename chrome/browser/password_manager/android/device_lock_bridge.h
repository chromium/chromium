// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_DEVICE_LOCK_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_DEVICE_LOCK_BRIDGE_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"

namespace ui {
class WindowAndroid;
}

// The glue for the Java-side implementation of DeviceLockBridge.
class DeviceLockBridge {
 public:
  DeviceLockBridge();
  virtual ~DeviceLockBridge();

  using DeviceLockConfirmedCallback = base::OnceCallback<void(bool)>;

  // Launches the Device Lock setup UI (explainer dialog and PIN/password setup
  // flow) before allowing users to continue with the saving passwords flow if
  // their device is not secure (ex: no PIN or password set). Password will only
  // be saved if users successfully set a PIN/password.
  virtual void LaunchDeviceLockUiBeforeSavingPassword(
      ui::WindowAndroid* window_android,
      DeviceLockConfirmedCallback callback);

  // Invokes a callback to save a pending password (if device lock was set up)
  // and clean up pointers and other data.
  void OnDeviceLockUiFinished(JNIEnv* env, bool is_device_lock_set);

  // Returns true iff the device requires a device lock (ex: pin/password) to
  // save passwords and does not have one set.
  virtual bool ShowDeviceLockUiBeforeSavingPassword();

  // Returns true iff a device lock (ex: pin/password) is needed to save
  // passwords.
  virtual bool RequiresDeviceLock();

 private:
  // Returns true iff the device has a device lock (ex: pin/password).
  bool IsDeviceSecure();

  // This object is an instance of DeviceLockBridge.java (the Java counterpart
  // to this class).
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // This callback should be a function call to
  // &SaveUpdatePasswordMessageDelegate::SavePasswordAfterDeviceLockActivity
  // after DeviceLockActivity.java has finished.
  DeviceLockConfirmedCallback device_lock_confirmed_callback_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_DEVICE_LOCK_BRIDGE_H_
