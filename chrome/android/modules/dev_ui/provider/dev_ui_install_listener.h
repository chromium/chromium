// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ANDROID_MODULES_DEV_UI_PROVIDER_DEV_UI_INSTALL_LISTENER_H_
#define CHROME_ANDROID_MODULES_DEV_UI_PROVIDER_DEV_UI_INSTALL_LISTENER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/callback.h"

namespace dev_ui {

// Install listener that receives install signal from Java.
//
// Self-destructs after run to simulate the behavior of being owned by Java.
class DevUiInstallListener {
 public:
  static DevUiInstallListener* Create(
      base::OnceCallback<void(bool)> on_complete);

  // Called by Java.
  void OnComplete(JNIEnv* env, bool success);

  base::android::ScopedJavaGlobalRef<jobject> j_listener() {
    return j_listener_;
  }

 private:
  explicit DevUiInstallListener(base::OnceCallback<void(bool)> on_complete);
  ~DevUiInstallListener();
  DevUiInstallListener(const DevUiInstallListener&) = delete;
  DevUiInstallListener& operator=(const DevUiInstallListener&) = delete;

  base::android::ScopedJavaGlobalRef<jobject> j_listener_;
  base::OnceCallback<void(bool)> on_complete_;
};

}  // namespace dev_ui

#endif  // CHROME_ANDROID_MODULES_DEV_UI_PROVIDER_DEV_UI_INSTALL_LISTENER_H_
