// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/assistant_header_delegate.h"

#include "chrome/android/features/autofill_assistant/jni_headers/AssistantHeaderDelegate_jni.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"

using base::android::AttachCurrentThread;

namespace autofill_assistant {

AssistantHeaderDelegate::AssistantHeaderDelegate(
    UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_assistant_header_delegate_ = Java_AssistantHeaderDelegate_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantHeaderDelegate::~AssistantHeaderDelegate() {
  Java_AssistantHeaderDelegate_clearNativePtr(AttachCurrentThread(),
                                              java_assistant_header_delegate_);
}

void AssistantHeaderDelegate::OnFeedbackButtonClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->OnFeedbackButtonClicked();
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantHeaderDelegate::GetJavaObject() {
  return java_assistant_header_delegate_;
}

}  // namespace autofill_assistant
