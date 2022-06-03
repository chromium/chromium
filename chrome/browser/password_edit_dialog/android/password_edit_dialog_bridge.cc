// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_edit_dialog/android/password_edit_dialog_bridge.h"

#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/password_edit_dialog/android/jni_headers/PasswordEditDialogBridge_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

PasswordEditDialog::~PasswordEditDialog() = default;

// static
std::unique_ptr<PasswordEditDialog> PasswordEditDialogBridge::Create(
    content::WebContents* web_contents,
    DialogAcceptedCallback dialog_accepted_callback,
    DialogDismissedCallback dialog_dismissed_callback) {
  DCHECK(web_contents);

  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();
  if (!window_android)
    return nullptr;
  return base::WrapUnique(new PasswordEditDialogBridge(
      window_android->GetJavaObject(), std::move(dialog_accepted_callback),
      std::move(dialog_dismissed_callback)));
}

PasswordEditDialogBridge::PasswordEditDialogBridge(
    base::android::ScopedJavaLocalRef<jobject> j_window_android,
    DialogAcceptedCallback dialog_accepted_callback,
    DialogDismissedCallback dialog_dismissed_callback)
    : dialog_accepted_callback_(std::move(dialog_accepted_callback)),
      dialog_dismissed_callback_(std::move(dialog_dismissed_callback)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_password_dialog_ = Java_PasswordEditDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this), j_window_android);
}

PasswordEditDialogBridge::~PasswordEditDialogBridge() {
  DCHECK(java_password_dialog_.is_null());
}

void PasswordEditDialogBridge::Show(
    const std::vector<std::u16string>& usernames,
    int selected_username_index,
    const std::u16string& password,
    const std::u16string& origin,
    const std::string& account_email) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobjectArray> j_usernames =
      base::android::ToJavaArrayOfStrings(env, usernames);

  base::android::ScopedJavaLocalRef<jstring> j_password =
      base::android::ConvertUTF16ToJavaString(env, password);
  base::android::ScopedJavaLocalRef<jstring> j_origin =
      base::android::ConvertUTF16ToJavaString(env, origin);
  base::android::ScopedJavaLocalRef<jstring> j_account_email =
      base::android::ConvertUTF8ToJavaString(env, account_email);

  Java_PasswordEditDialogBridge_show(env, java_password_dialog_, j_usernames,
                                     selected_username_index, j_password,
                                     j_origin, j_account_email);
}

void PasswordEditDialogBridge::Dismiss() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PasswordEditDialogBridge_dismiss(env, java_password_dialog_);
}

void PasswordEditDialogBridge::OnDialogAccepted(JNIEnv* env,
                                                jint selected_username_index) {
  std::move(dialog_accepted_callback_).Run(selected_username_index);
}

void PasswordEditDialogBridge::OnDialogDismissed(JNIEnv* env,
                                                 jboolean dialogAccepted) {
  java_password_dialog_.Reset();
  std::move(dialog_dismissed_callback_).Run(dialogAccepted);
}
