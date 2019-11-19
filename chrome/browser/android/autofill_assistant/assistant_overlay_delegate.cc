// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/assistant_overlay_delegate.h"

#include "chrome/android/features/autofill_assistant/jni_headers/AssistantOverlayDelegate_jni.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"

using base::android::AttachCurrentThread;

namespace autofill_assistant {

AssistantOverlayDelegate::AssistantOverlayDelegate(
    UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_assistant_overlay_delegate_ = Java_AssistantOverlayDelegate_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantOverlayDelegate::~AssistantOverlayDelegate() {
  Java_AssistantOverlayDelegate_clearNativePtr(
      AttachCurrentThread(), java_assistant_overlay_delegate_);
}

void AssistantOverlayDelegate::OnUnexpectedTaps(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->OnUnexpectedTaps();
}

void AssistantOverlayDelegate::UpdateTouchableArea(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->UpdateTouchableArea();
}

void AssistantOverlayDelegate::OnUserInteractionInsideTouchableArea(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->OnUserInteractionInsideTouchableArea();
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantOverlayDelegate::GetJavaObject() {
  return java_assistant_overlay_delegate_;
}

}  // namespace autofill_assistant
