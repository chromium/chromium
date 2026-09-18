// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_edit_dialog/android/password_edit_dialog_bridge.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/default_conversions.h"
#include "ui/android/window_android.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/password_edit_dialog/android/jni_headers/PasswordEditDialogBridge_jni.h"

PasswordEditDialog::~PasswordEditDialog() = default;

// static
std::unique_ptr<PasswordEditDialog> PasswordEditDialogBridge::Create(
    content::WebContents* web_contents,
    PasswordEditDialogBridgeDelegate* delegate) {
  DCHECK(web_contents);
  CHECK(delegate);

  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();
  if (!window_android) {
    return nullptr;
  }
  return base::WrapUnique(
      new PasswordEditDialogBridge(window_android->GetJavaObject(), delegate));
}

PasswordEditDialogBridge::PasswordEditDialogBridge(
    base::android::ScopedJavaLocalRef<jobject> j_window_android,
    PasswordEditDialogBridgeDelegate* delegate)
    : delegate_(delegate) {
  CHECK(delegate_);

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
    const std::optional<std::string>& account_email,
    bool is_saving_blocked_by_trusted_vault_error) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_PasswordEditDialogBridge_showPasswordEditDialog(
      env, java_password_dialog_, saved_usernames, username, password,
      account_email, is_saving_blocked_by_trusted_vault_error);
}

void PasswordEditDialogBridge::Dismiss() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PasswordEditDialogBridge_dismiss(env, java_password_dialog_);
}

void PasswordEditDialogBridge::OnDialogAccepted(
    JNIEnv* env,
    const std::u16string& username,
    const std::u16string& password) {
  delegate_->HandleSavePasswordFromDialog(username, password);
}

void PasswordEditDialogBridge::OnDialogDismissed(JNIEnv* env,
                                                 bool dialogAccepted) {
  java_password_dialog_.Reset();
  delegate_->HandleDialogDismissed(dialogAccepted);
}

bool PasswordEditDialogBridge::IsUsingAccountStorage(
    JNIEnv* env,
    const std::u16string& username) {
  return delegate_->IsUsingAccountStorage(username);
}

DEFINE_JNI(PasswordEditDialogBridge)
