// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_input_connection.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/features/vr/jni_headers/TextEditAction_jni.h"
#include "chrome/android/features/vr/jni_headers/VrInputConnection_jni.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace vr {

VrInputConnection::VrInputConnection(content::WebContents* web_contents) {
  DCHECK(web_contents);
  JNIEnv* env = AttachCurrentThread();
  j_object_.Reset(Java_VrInputConnection_create(
      env, reinterpret_cast<jlong>(this), web_contents->GetJavaWebContents()));
}

VrInputConnection::~VrInputConnection() {}

void VrInputConnection::OnKeyboardEdit(const TextEdits& edits) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto java_edit_array = Java_TextEditAction_createArray(env, edits.size());
  int index = 0;
  for (const auto& edit : edits) {
    auto text = base::android::ConvertUTF16ToJavaString(env, edit.text());
    auto java_edit = Java_TextEditAction_Constructor(env, edit.type(), text,
                                                     edit.cursor_position());
    env->SetObjectArrayElement(java_edit_array.obj(), index, java_edit.obj());
    index++;
  }
  Java_VrInputConnection_onKeyboardEdit(env, j_object_, java_edit_array);
}

void VrInputConnection::SubmitInput() {
  Java_VrInputConnection_submitInput(base::android::AttachCurrentThread(),
                                     j_object_);
}

void VrInputConnection::RequestTextState(TextStateUpdateCallback callback) {
  text_state_update_callbacks_.emplace(std::move(callback));
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_VrInputConnection_requestTextState(env, j_object_);
}

void VrInputConnection::UpdateTextState(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jstring jtext) {
  DCHECK(!text_state_update_callbacks_.empty());
  std::string text;
  base::android::ConvertJavaStringToUTF8(env, jtext, &text);
  auto text_state_update_callback =
      std::move(text_state_update_callbacks_.front());
  text_state_update_callbacks_.pop();
  std::move(text_state_update_callback).Run(base::UTF8ToUTF16(text));
}

base::android::ScopedJavaLocalRef<jobject> VrInputConnection::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(j_object_);
}

}  // namespace vr
