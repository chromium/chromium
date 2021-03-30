// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback_helpers.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreBridge_jni.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreCredential_jni.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "url/android/gurl_android.h"

namespace {
using password_manager::PasswordForm;
using SavedPasswordsView =
    password_manager::SavedPasswordsPresenter::SavedPasswordsView;

PasswordForm ConvertJavaObjectToPasswordForm(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  PasswordForm form;

  form.url = *url::GURLAndroid::ToNativeGURL(
      env, Java_PasswordStoreCredential_getUrl(env, credential));
  form.signon_realm = password_manager::GetSignonRealm(form.url);
  form.username_value = ConvertJavaStringToUTF16(
      env, Java_PasswordStoreCredential_getUsername(env, credential));
  form.password_value = ConvertJavaStringToUTF16(
      env, Java_PasswordStoreCredential_getPassword(env, credential));

  return form;
}

}  // namespace

// static
static jlong JNI_PasswordStoreBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_bridge) {
  return reinterpret_cast<intptr_t>(new PasswordStoreBridge(java_bridge));
}

PasswordStoreBridge::PasswordStoreBridge(
    const base::android::JavaParamRef<jobject>& java_bridge)
    : java_bridge_(java_bridge) {
  saved_passwords_presenter_.Init();
  observed_saved_password_presenter_.Observe(&saved_passwords_presenter_);
}

PasswordStoreBridge::~PasswordStoreBridge() = default;

void PasswordStoreBridge::InsertPasswordCredential(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  password_store_->AddLogin(ConvertJavaObjectToPasswordForm(env, credential));
}

bool PasswordStoreBridge::EditPassword(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential,
    const base::android::JavaParamRef<jstring>& new_password) {
  return saved_passwords_presenter_.EditPassword(
      ConvertJavaObjectToPasswordForm(env, credential),
      ConvertJavaStringToUTF16(env, new_password));
}

jint PasswordStoreBridge::GetPasswordStoreCredentialsCount(JNIEnv* env) const {
  return static_cast<int>(
      saved_passwords_presenter_.GetSavedPasswords().size());
}

void PasswordStoreBridge::GetAllCredentials(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& java_credentials) const {
  const auto credentials = saved_passwords_presenter_.GetSavedPasswords();
  for (size_t i = 0; i < credentials.size(); ++i) {
    const auto& credential = credentials[i];
    Java_PasswordStoreBridge_insertCredential(
        env, java_credentials, i,
        url::GURLAndroid::FromNativeGURL(env, credential.url),
        base::android::ConvertUTF16ToJavaString(env, credential.username_value),
        base::android::ConvertUTF16ToJavaString(env,
                                                credential.password_value));
  }
}

void PasswordStoreBridge::ClearAllPasswords(JNIEnv* env) {
  password_store_->ClearStore(
      base::BindOnce(&PasswordStoreBridge::OnPasswordStoreCleared,
                     weak_factory_.GetWeakPtr()));
}

void PasswordStoreBridge::OnPasswordStoreCleared(bool success) {
  if (success) {
    saved_passwords_presenter_.Init();
  }
}

void PasswordStoreBridge::Destroy(JNIEnv* env) {
  delete this;
}

void PasswordStoreBridge::OnSavedPasswordsChanged(
    SavedPasswordsView passwords) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Notifies java counter side that a new set of credentials is available.
  Java_PasswordStoreBridge_passwordListAvailable(
      env, java_bridge_, static_cast<int>(passwords.size()));
}

void PasswordStoreBridge::OnEdited(const PasswordForm& form) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Notifies java counter side that a credential has been edited.
  Java_PasswordStoreBridge_onEditCredential(
      env, java_bridge_,
      Java_PasswordStoreBridge_createPasswordStoreCredential(
          env, url::GURLAndroid::FromNativeGURL(env, form.url),
          base::android::ConvertUTF16ToJavaString(env, form.username_value),
          base::android::ConvertUTF16ToJavaString(env, form.password_value)));
}
