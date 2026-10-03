// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/glic_helper_android.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/GlicHelper_jni.h"

namespace glic {

namespace {

constexpr char kRecordAudioPermission[] = "android.permission.RECORD_AUDIO";

}  // namespace

void MicPermissionUi::ShowMicPermissionDialog(
    ui::WindowAndroid* window_android,
    base::OnceCallback<void(bool)> callback) {
  if (window_android && window_android->GetJavaObject()) {
    Java_GlicHelper_showMicPermissionDialog(jni_zero::AttachCurrentThread(),
                                            window_android->GetJavaObject(),
                                            std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

bool MicPermissionUi::HasMicOsPermission(ui::WindowAndroid* window_android) {
  return window_android &&
         window_android->HasPermission(kRecordAudioPermission);
}

void MicPermissionUi::RequestMicOsPermission(
    ui::WindowAndroid* window_android,
    base::OnceCallback<void(bool)> callback) {
  if (window_android && window_android->GetJavaObject()) {
    Java_GlicHelper_requestMicOsPermission(jni_zero::AttachCurrentThread(),
                                           window_android->GetJavaObject(),
                                           std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void MicPermissionUi::ShowMicDisabledSnackbar(
    ui::WindowAndroid* window_android) {
  if (window_android && window_android->GetJavaObject()) {
    Java_GlicHelper_showMicDisabledSnackbar(jni_zero::AttachCurrentThread(),
                                            window_android->GetJavaObject());
  }
}

}  // namespace glic
