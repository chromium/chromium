// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_edit_dialog/android/password_edit_dialog_bridge.h"
#include <jni.h>

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
    LegacyDialogAcceptedCallback legacy_dialog_accepted_callback,
    DialogDismissedCallback dialog_dismissed_callback) {
  DCHECK(web_contents);

  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();
  if (!window_android)
    return nullptr;
  return base::WrapUnique(new PasswordEditDialogBridge(
      window_android->GetJavaObject(), std::move(dialog_accepted_callback),
      std::move(legacy_dialog_accepted_callback),
      std::move(dialog_dismissed_callback)));
}

PasswordEditDialogBridge::PasswordEditDialogBridge(
    base::android::ScopedJavaLocalRef<jobject> j_window_android,
    DialogAcceptedCallback dialog_accepted_callback,
    LegacyDialogAcceptedCallback legacy_dialog_accepted_callback,
    DialogDismissedCallback dialog_dismissed_callback)
    : dialog_accepted_callback_(std::move(dialog_accepted_callback)),
      legacy_dialog_accepted_callback_(
          std::move(legacy_dialog_accepted_callback)),
      dialog_dismissed_callback_(std::move(dialog_dismissed_callback)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_password_dialog_ = Java_PasswordEditDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this), j_window_android);
}

PasswordEditDialogBridge::~PasswordEditDialogBridge() {
  DCHECK(java_password_dialog_.is_null());
}

void PasswordEditDialogBridge::ShowPasswordEditDialog(
    const std::vector<std::u16string>& saved_usernames,
    const std::u16string& username,
    const std::u16string& password,
    const std::string& account_email) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobjectArray> j_saved_usernames =
      base::android::ToJavaArrayOfStrings(env, saved_usernames);
  base::android::ScopedJavaLocalRef<jstring> j_username =
      base::android::ConvertUTF16ToJavaString(env, username);
  base::android::ScopedJavaLocalRef<jstring> j_password =
      base::android::ConvertUTF16ToJavaString(env, password);
  base::android::ScopedJavaLocalRef<jstring> j_account_email =
      base::android::ConvertUTF8ToJavaString(env, account_email);

  Java_PasswordEditDialogBridge_showPasswordEditDialog(
      env, java_password_dialog_, j_saved_usernames, j_username, j_password,
      j_account_email);
}

void PasswordEditDialogBridge::ShowLegacyPasswordEditDialog(
    const std::vector<std::u16string>& usernames,
    int selected_username_index,
    const std::string& account_email) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobjectArray> j_saved_usernames =
      base::android::ToJavaArrayOfStrings(env, usernames);
  base::android::ScopedJavaLocalRef<jstring> j_account_email =
      base::android::ConvertUTF8ToJavaString(env, account_email);

  Java_PasswordEditDialogBridge_showLegacyPasswordEditDialog(
      env, java_password_dialog_, j_saved_usernames, selected_username_index,
      j_account_email);
}

void PasswordEditDialogBridge::Dismiss() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PasswordEditDialogBridge_dismiss(env, java_password_dialog_);
}

void PasswordEditDialogBridge::OnDialogAccepted(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& username,
    const base::android::JavaParamRef<jstring>& password) {
  std::move(dialog_accepted_callback_)
      .Run(base::android::ConvertJavaStringToUTF16(username),
           base::android::ConvertJavaStringToUTF16(password));
}

void PasswordEditDialogBridge::OnLegacyDialogAccepted(JNIEnv* env,
                                                      jint username_index) {
  std::move(legacy_dialog_accepted_callback_).Run(username_index);
}

void PasswordEditDialogBridge::OnDialogDismissed(JNIEnv* env,
                                                 jboolean dialogAccepted) {
  java_password_dialog_.Reset();
  std::move(dialog_dismissed_callback_).Run(dialogAccepted);
}
