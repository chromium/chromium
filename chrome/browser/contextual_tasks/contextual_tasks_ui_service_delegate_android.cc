// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/contextual_tasks/jni_headers/ContextualTasksUiServiceDelegate_jni.h"

namespace contextual_tasks {

ContextualTasksUiServiceDelegateAndroid::
    ContextualTasksUiServiceDelegateAndroid(Profile* profile)
    : ContextualTasksUiServiceDelegate() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(env, Java_ContextualTasksUiServiceDelegate_create(
                                env, profile->GetJavaObject()));
}

ContextualTasksUiServiceDelegateAndroid::
    ~ContextualTasksUiServiceDelegateAndroid() = default;

void ContextualTasksUiServiceDelegateAndroid::OpenFeedbackUi(
    BrowserWindowInterface* browser_window_interface,
    const GURL& page_url) {
  if (!browser_window_interface || !browser_window_interface->GetWindow() ||
      !browser_window_interface->GetWindow()->GetNativeWindow()) {
    return;
  }
  ui::WindowAndroid* window_android =
      browser_window_interface->GetWindow()->GetNativeWindow();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksUiServiceDelegate_openFeedbackUi(
      env, java_delegate_, window_android->GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, page_url.spec()));
}

}  // namespace contextual_tasks
