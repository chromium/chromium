// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/flags/bad_flags_snackbar_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/BadFlagsSnackbarManager_jni.h"

void ShowBadFlagsSnackbar(content::WebContents* web_contents,
                          const std::u16string& message) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents->GetNativeView();
  DCHECK(view_android);
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android)
    return;
  chrome::Java_BadFlagsSnackbarManager_show(
      env, window_android->GetJavaObject(),
      base::android::ConvertUTF16ToJavaString(env, message));
}
