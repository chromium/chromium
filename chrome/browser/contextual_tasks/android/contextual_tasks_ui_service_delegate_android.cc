// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_ui_service_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
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
    : ContextualTasksUiServiceDelegate(), profile_(profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(env,
                       Java_ContextualTasksUiServiceDelegate_create(
                           env, reinterpret_cast<intptr_t>(this), profile));
}

ContextualTasksUiServiceDelegateAndroid::
    ~ContextualTasksUiServiceDelegateAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksUiServiceDelegate_clearNativePtr(env, java_delegate_);
}

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
      env, java_delegate_, window_android, page_url.spec());
}

void ContextualTasksUiServiceDelegateAndroid::OnWebUIReady(
    const base::Uuid& task_id,
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksUiServiceDelegate_onWebUIReady(
      env, java_delegate_, task_id.AsLowercaseString(), web_contents);
}

void ContextualTasksUiServiceDelegateAndroid::ShowUndoSnackbar(
    BrowserWindowInterface* browser_window_interface) {
  if (!browser_window_interface || !browser_window_interface->GetWindow() ||
      !browser_window_interface->GetWindow()->GetNativeWindow()) {
    return;
  }
  ui::WindowAndroid* window_android =
      browser_window_interface->GetWindow()->GetNativeWindow();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksUiServiceDelegate_showUndoSnackbar(
      env, java_delegate_, window_android,
      reinterpret_cast<intptr_t>(browser_window_interface));
}

void ContextualTasksUiServiceDelegateAndroid::UndoClose(
    JNIEnv* env,
    int64_t browser_window_ptr) {
  auto* browser_window =
      reinterpret_cast<BrowserWindowInterface*>(browser_window_ptr);
  if (browser_window) {
    auto* controller = ContextualTasksPanelController::From(browser_window);
    if (controller) {
      controller->Show();
    }
  }
}

}  // namespace contextual_tasks

DEFINE_JNI(ContextualTasksUiServiceDelegate)
