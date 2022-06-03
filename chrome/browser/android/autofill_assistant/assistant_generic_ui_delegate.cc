// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/assistant_generic_ui_delegate.h"

#include "base/android/jni_string.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantGenericUiDelegate_jni.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "components/autofill_assistant/browser/service.pb.h"

using base::android::AttachCurrentThread;

namespace autofill_assistant {

AssistantGenericUiDelegate::AssistantGenericUiDelegate(
    UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_assistant_generic_ui_delegate_ = Java_AssistantGenericUiDelegate_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantGenericUiDelegate::~AssistantGenericUiDelegate() {
  Java_AssistantGenericUiDelegate_clearNativePtr(
      AttachCurrentThread(), java_assistant_generic_ui_delegate_);
}

void AssistantGenericUiDelegate::OnViewClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jview_identifier) {
  std::string view_identifier;
  if (jview_identifier) {
    base::android::ConvertJavaStringToUTF8(env, jview_identifier,
                                           &view_identifier);
  }

  ui_controller_->OnViewEvent({EventProto::kOnViewClicked, view_identifier});
}

void AssistantGenericUiDelegate::OnValueChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jmodel_identifier,
    const base::android::JavaParamRef<jobject>& jvalue) {
  ui_controller_->OnValueChanged(
      ui_controller_android_utils::SafeConvertJavaStringToNative(
          env, jmodel_identifier),
      ui_controller_android_utils::ToNativeValue(env, jvalue));
}

void AssistantGenericUiDelegate::OnTextLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jlink) {
  ui_controller_->OnViewEvent(
      {EventProto::kOnTextLinkClicked, base::NumberToString(jlink)});
}

void AssistantGenericUiDelegate::OnGenericPopupDismissed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jpopup_identifier) {
  ui_controller_->OnViewEvent(
      {EventProto::kOnPopupDismissed,
       ui_controller_android_utils::SafeConvertJavaStringToNative(
           env, jpopup_identifier)});
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantGenericUiDelegate::GetJavaObject() {
  return java_assistant_generic_ui_delegate_;
}

void AssistantGenericUiDelegate::OnViewContainerCleared(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jview_identifier) {
  ui_controller_->OnViewEvent(
      {EventProto::kOnViewContainerCleared,
       ui_controller_android_utils::SafeConvertJavaStringToNative(
           env, jview_identifier)});
}

}  // namespace autofill_assistant
