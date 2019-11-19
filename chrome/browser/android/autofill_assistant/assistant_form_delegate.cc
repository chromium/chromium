// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/assistant_form_delegate.h"

#include "chrome/android/features/autofill_assistant/jni_headers/AssistantFormDelegate_jni.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"

using base::android::AttachCurrentThread;

namespace autofill_assistant {

AssistantFormDelegate::AssistantFormDelegate(UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_assistant_form_delegate_ = Java_AssistantFormDelegate_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantFormDelegate::~AssistantFormDelegate() {
  Java_AssistantFormDelegate_clearNativePtr(AttachCurrentThread(),
                                            java_assistant_form_delegate_);
}

void AssistantFormDelegate::OnCounterChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint input_index,
    jint counter_index,
    jint value) {
  ui_controller_->OnCounterChanged(input_index, counter_index, value);
}

void AssistantFormDelegate::OnChoiceSelectionChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint input_index,
    jint choice_index,
    jboolean selected) {
  ui_controller_->OnChoiceSelectionChanged(input_index, choice_index, selected);
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantFormDelegate::GetJavaObject() {
  return java_assistant_form_delegate_;
}

}  // namespace autofill_assistant
