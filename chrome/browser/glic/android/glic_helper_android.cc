// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/glic_helper_android.h"

#include "base/android/jni_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/glic/android/jni_headers/GlicHelper_jni.h"

namespace glic {

void ShowMicDisabledSnackbar(ui::WindowAndroid* window_android) {
  if (window_android && window_android->GetJavaObject()) {
    Java_GlicHelper_showMicDisabledSnackbar(
        base::android::AttachCurrentThread(), window_android->GetJavaObject());
  }
}

}  // namespace glic
