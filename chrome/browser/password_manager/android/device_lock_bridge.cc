// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/device_lock_bridge.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/DeviceLockBridge_jni.h"
#include "chrome/browser/password_manager/android/save_update_password_message_delegate.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;

DeviceLockBridge::DeviceLockBridge() {
  java_object_ = Java_DeviceLockBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

DeviceLockBridge::~DeviceLockBridge() {
  Java_DeviceLockBridge_clearNativePointer(base::android::AttachCurrentThread(),
                                           java_object_);
}

void DeviceLockBridge::LaunchDeviceLockUiBeforeSavingPassword(
    ui::WindowAndroid* window_android,
    DeviceLockConfirmedCallback callback) {
  CHECK(window_android) << "Can be null before creation and during clean-up";
  CHECK(callback);
  device_lock_confirmed_callback_ = std::move(callback);
  Java_DeviceLockBridge_launchDeviceLockUiBeforeSavingPassword(
      base::android::AttachCurrentThread(), java_object_,
      window_android->GetJavaObject());
}

void DeviceLockBridge::OnDeviceLockUiFinished(JNIEnv* env,
                                              bool is_device_lock_set) {
  std::move(device_lock_confirmed_callback_).Run(is_device_lock_set);
}

bool DeviceLockBridge::ShowDeviceLockUiBeforeSavingPassword() {
  return RequiresDeviceLock() && !IsDeviceSecure();
}

bool DeviceLockBridge::RequiresDeviceLock() {
  return base::android::BuildInfo::GetInstance()->is_automotive();
}

bool DeviceLockBridge::IsDeviceSecure() {
  return Java_DeviceLockBridge_isDeviceSecure(
      base::android::AttachCurrentThread(), java_object_);
}
