// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_editing_bridge.h"

#include <memory>

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/PasswordEditingBridge_jni.h"
#include "chrome/browser/android/password_edit_delegate_settings_impl.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

void PasswordEditingBridge::Destroy(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  delete this;
}

void PasswordEditingBridge::LaunchPasswordEntryEditor(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& context,
    Profile* profile,
    base::span<const std::unique_ptr<autofill::PasswordForm>> forms_to_change,
    std::vector<base::string16> existing_usernames) {
  // |forms_to_change| is built in
  // |PasswordManagerPresnter::TryGetPasswordForms|, which under certain
  // circumstances can return an empty array (e.g. when the index for
  // the requested forms entry in the forms map is out of bounds because the
  // entry was removed in the meantime).
  if (forms_to_change.empty())
    return;

  // PasswordEditingBridge will destroy itself when the UI is gone on the Java
  // side.
  PasswordEditingBridge* password_editing_bridge = new PasswordEditingBridge(
      std::make_unique<PasswordEditDelegateSettingsImpl>(
          profile, forms_to_change, std::move(existing_usernames)));

  Java_PasswordEditingBridge_showEditingUI(
      base::android::AttachCurrentThread(),
      password_editing_bridge->java_object_, context,
      ConvertUTF8ToJavaString(env, forms_to_change[0]->signon_realm),
      ConvertUTF16ToJavaString(env, forms_to_change[0]->username_value),
      ConvertUTF16ToJavaString(env, forms_to_change[0]->password_value));
}

void PasswordEditingBridge::HandleEditSavedPasswordEntry(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jstring>& new_username,
    const JavaParamRef<jstring>& new_password) {
  password_edit_delegate_->EditSavedPassword(
      ConvertJavaStringToUTF16(env, new_username),
      ConvertJavaStringToUTF16(env, new_password));
}

PasswordEditingBridge::PasswordEditingBridge(
    std::unique_ptr<PasswordEditDelegate> password_edit_delegate)
    : password_edit_delegate_(std::move(password_edit_delegate)) {
  java_object_.Reset(Java_PasswordEditingBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
}

PasswordEditingBridge::~PasswordEditingBridge() = default;
