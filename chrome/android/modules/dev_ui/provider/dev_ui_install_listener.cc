// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/modules/dev_ui/provider/dev_ui_install_listener.h"

#include <utility>

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/modules/dev_ui/provider/jni_headers/DevUiInstallListener_jni.h"

namespace dev_ui {

// static
DevUiInstallListener* DevUiInstallListener::Create(
    base::OnceCallback<void(bool)> on_complete) {
  return new DevUiInstallListener(std::move(on_complete));
}

void DevUiInstallListener::OnComplete(JNIEnv* env, bool success) {
  std::move(on_complete_).Run(success);
  delete this;
}

DevUiInstallListener::DevUiInstallListener(
    base::OnceCallback<void(bool)> on_complete)
    : j_listener_(Java_DevUiInstallListener_Constructor(
          base::android::AttachCurrentThread(),
          reinterpret_cast<jlong>(this))),
      on_complete_(std::move(on_complete)) {}

DevUiInstallListener::~DevUiInstallListener() {
  Java_DevUiInstallListener_onNativeDestroy(
      base::android::AttachCurrentThread(), j_listener_);
}

}  // namespace dev_ui
