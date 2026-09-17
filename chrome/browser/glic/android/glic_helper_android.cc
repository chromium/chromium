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

void ShowMicDisabledSnackbar(ui::WindowAndroid* window_android) {
  if (window_android && window_android->GetJavaObject()) {
    Java_GlicHelper_showMicDisabledSnackbar(jni_zero::AttachCurrentThread(),
                                            window_android->GetJavaObject());
  }
}

void ShowMicPermissionDialog(ui::WindowAndroid* window_android,
                             base::OnceCallback<void(bool)> callback) {
  if (window_android && window_android->GetJavaObject()) {
    Java_GlicHelper_showMicPermissionDialog(jni_zero::AttachCurrentThread(),
                                            window_android->GetJavaObject(),
                                            std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

}  // namespace glic
