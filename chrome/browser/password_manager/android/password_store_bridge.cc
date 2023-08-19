// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreBridge_jni.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreCredential_jni.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "url/android/gurl_android.h"

namespace {
using password_manager::PasswordForm;

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

void PasswordStoreBridge::InsertPasswordCredentialForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  profile_store_->AddLogin(ConvertJavaObjectToPasswordForm(env, credential));
}

bool PasswordStoreBridge::EditPassword(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential,
    const base::android::JavaParamRef<jstring>& new_password) {
  password_manager::CredentialUIEntry original_credential(
      ConvertJavaObjectToPasswordForm(env, credential));
  password_manager::CredentialUIEntry updated_credential = original_credential;
  updated_credential.password = ConvertJavaStringToUTF16(env, new_password);
  return saved_passwords_presenter_.EditSavedCredentials(original_credential,
                                                         updated_credential) ==
         password_manager::SavedPasswordsPresenter::EditResult::kSuccess;
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
        url::GURLAndroid::FromNativeGURL(env, credential.GetURL()),
        base::android::ConvertUTF16ToJavaString(env, credential.username),
        base::android::ConvertUTF16ToJavaString(env, credential.password));
  }
}

void PasswordStoreBridge::ClearAllPasswords(JNIEnv* env) {
  profile_store_->RemoveLoginsCreatedBetween(base::Time(), base::Time::Max());
}

void PasswordStoreBridge::Destroy(JNIEnv* env) {
  delete this;
}

void PasswordStoreBridge::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Notifies java counter side that a new set of credentials is available.
  Java_PasswordStoreBridge_passwordListAvailable(
      env, java_bridge_,
      static_cast<int>(
          saved_passwords_presenter_.GetSavedCredentials().size()));
}

void PasswordStoreBridge::OnEdited(
    const password_manager::CredentialUIEntry& credential) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Notifies java counter side that a credential has been edited.
  Java_PasswordStoreBridge_onEditCredential(
      env, java_bridge_,
      Java_PasswordStoreBridge_createPasswordStoreCredential(
          env, url::GURLAndroid::FromNativeGURL(env, credential.GetURL()),
          base::android::ConvertUTF16ToJavaString(env, credential.username),
          base::android::ConvertUTF16ToJavaString(env, credential.password)));
}
