// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/password_check_bridge.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "chrome/browser/password_check/android/jni_headers/CompromisedCredential_jni.h"
#include "chrome/browser/password_check/android/jni_headers/PasswordCheckBridge_jni.h"
#include "chrome/browser/password_manager/android/password_checkup_launcher_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "url/android/gurl_android.h"

namespace {

password_manager::CredentialView ConvertJavaObjectToCredentialView(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  return password_manager::CredentialView(
      ConvertJavaStringToUTF8(
          env, Java_CompromisedCredential_getSignonRealm(env, credential)),
      *url::GURLAndroid::ToNativeGURL(
          env, Java_CompromisedCredential_getOrigin(env, credential)),
      ConvertJavaStringToUTF16(
          env, Java_CompromisedCredential_getUsername(env, credential)),
      ConvertJavaStringToUTF16(
          env, Java_CompromisedCredential_getPassword(env, credential)));
}

}  // namespace

static jlong JNI_PasswordCheckBridge_Create(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_bridge) {
  return reinterpret_cast<intptr_t>(new PasswordCheckBridge(java_bridge));
}

PasswordCheckBridge::PasswordCheckBridge(
    const base::android::JavaParamRef<jobject>& java_bridge)
    : java_bridge_(java_bridge) {}

PasswordCheckBridge::~PasswordCheckBridge() = default;

void PasswordCheckBridge::StartCheck(JNIEnv* env) {
  check_manager_.StartCheck();
}

void PasswordCheckBridge::StopCheck(JNIEnv* env) {
  check_manager_.StopCheck();
}

int64_t PasswordCheckBridge::GetLastCheckTimestamp(JNIEnv* env) {
  return check_manager_.GetLastCheckTimestamp().ToJavaTime();
}

jint PasswordCheckBridge::GetCompromisedCredentialsCount(JNIEnv* env) {
  return check_manager_.GetCompromisedCredentialsCount();
}

jint PasswordCheckBridge::GetSavedPasswordsCount(JNIEnv* env) {
  return check_manager_.GetSavedPasswordsCount();
}

void PasswordCheckBridge::GetCompromisedCredentials(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& java_credentials) {
  std::vector<PasswordCheckManager::CompromisedCredentialForUI> credentials =
      check_manager_.GetCompromisedCredentials();

  for (size_t i = 0; i < credentials.size(); ++i) {
    const auto& credential = credentials[i];
    Java_PasswordCheckBridge_insertCredential(
        env, java_credentials, i,
        base::android::ConvertUTF8ToJavaString(env, credential.signon_realm),
        url::GURLAndroid::FromNativeGURL(env, credential.url),
        base::android::ConvertUTF16ToJavaString(env, credential.username),
        base::android::ConvertUTF16ToJavaString(env, credential.display_origin),
        base::android::ConvertUTF16ToJavaString(env,
                                                credential.display_username),
        base::android::ConvertUTF16ToJavaString(env, credential.password),
        base::android::ConvertUTF8ToJavaString(env,
                                               credential.change_password_url),
        base::android::ConvertUTF8ToJavaString(env, credential.package_name),
        credential.create_time.ToJavaTime(),
        (credential.insecure_type ==
         password_manager::InsecureCredentialTypeFlags::kCredentialLeaked),
        (credential.insecure_type ==
         password_manager::InsecureCredentialTypeFlags::kCredentialPhished),
        credential.has_startable_script, credential.has_auto_change_button);
  }
}

void PasswordCheckBridge::LaunchCheckupInAccount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& activity) {
  PasswordCheckupLauncherHelper::LaunchCheckupInAccountWithActivity(
      env,
      base::android::ConvertUTF8ToJavaString(
          env, password_manager::GetPasswordCheckupURL().spec()),
      activity);
}

void PasswordCheckBridge::UpdateCredential(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential,
    const base::android::JavaParamRef<jstring>& new_password) {
  check_manager_.UpdateCredential(
      ConvertJavaObjectToCredentialView(env, credential),
      base::android::ConvertJavaStringToUTF8(new_password));
}

void PasswordCheckBridge::RemoveCredential(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  check_manager_.RemoveCredential(
      ConvertJavaObjectToCredentialView(env, credential));
}

void PasswordCheckBridge::Destroy(JNIEnv* env) {
  check_manager_.StopCheck();
  delete this;
}

bool PasswordCheckBridge::AreScriptsRefreshed(JNIEnv* env) const {
  return check_manager_.AreScriptsRefreshed();
}

void PasswordCheckBridge::RefreshScripts(JNIEnv* env) {
  check_manager_.RefreshScripts();
}

void PasswordCheckBridge::OnSavedPasswordsFetched(int count) {
  Java_PasswordCheckBridge_onSavedPasswordsFetched(
      base::android::AttachCurrentThread(), java_bridge_, count);
}

void PasswordCheckBridge::OnCompromisedCredentialsChanged(int count) {
  Java_PasswordCheckBridge_onCompromisedCredentialsFetched(
      base::android::AttachCurrentThread(), java_bridge_, count);
}

void PasswordCheckBridge::OnPasswordCheckStatusChanged(
    password_manager::PasswordCheckUIStatus status) {
  Java_PasswordCheckBridge_onPasswordCheckStatusChanged(
      base::android::AttachCurrentThread(), java_bridge_,
      static_cast<int>(status));
}

void PasswordCheckBridge::OnPasswordCheckProgressChanged(
    int already_processed,
    int remaining_in_queue) {
  Java_PasswordCheckBridge_onPasswordCheckProgressChanged(
      base::android::AttachCurrentThread(), java_bridge_, already_processed,
      remaining_in_queue);
}
