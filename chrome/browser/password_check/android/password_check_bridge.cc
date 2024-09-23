// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/password_check_bridge.h"

#include <jni.h>
#include <string>

#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/android/password_checkup_launcher_helper.h"
#include "chrome/browser/password_manager/android/password_checkup_launcher_helper_impl.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_check/android/internal/internal_jni/PasswordCheckBridge_jni.h"
#include "chrome/browser/password_check/android/jni_headers/CompromisedCredential_jni.h"

namespace {

using affiliations::FacetURI;

password_manager::CredentialUIEntry ConvertJavaObjectToCredential(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  std::string signon_realm = base::android::ConvertJavaStringToUTF8(
      env, Java_CompromisedCredential_getSignonRealm(env, credential));
  FacetURI facet = FacetURI::FromPotentiallyInvalidSpec(signon_realm);
  // For the UI, Android credentials store the affiliated realm in the
  // url field, however the saved credential should contains the signon realm
  // instead.
  GURL url = facet.IsValidAndroidFacetURI()
                 ? GURL(signon_realm)
                 : url::GURLAndroid::ToNativeGURL(
                       env, Java_CompromisedCredential_getAssociatedUrl(
                                env, credential));
  password_manager::CredentialUIEntry entry;

  password_manager::CredentialFacet credential_facet;
  credential_facet.url = std::move(url);
  credential_facet.signon_realm = std::move(signon_realm);
  entry.facets.push_back(std::move(credential_facet));

  entry.username = base::android::ConvertJavaStringToUTF16(
      env, Java_CompromisedCredential_getUsername(env, credential));
  entry.password = base::android::ConvertJavaStringToUTF16(
      env, Java_CompromisedCredential_getPassword(env, credential));
  entry.last_used_time = base::Time::FromMillisecondsSinceUnixEpoch(
      Java_CompromisedCredential_getLastUsedTime(env, credential));
  entry.stored_in.insert(password_manager::PasswordForm::Store::kProfileStore);
  return entry;
}

// Checks whether the credential is leaked but not phished. Other compromising
// states are ignored (e.g. weak or reused).
constexpr bool IsOnlyLeaked(
    const PasswordCheckManager::CompromisedCredentialForUI& credential) {
  return credential.IsLeaked() && !credential.IsPhished();
}

// Checks whether the credential is phished but not leaked. Other compromising
// states are ignored (e.g. weak or reused).
constexpr bool IsOnlyPhished(
    const PasswordCheckManager::CompromisedCredentialForUI& credential) {
  return credential.IsPhished() && !credential.IsLeaked();
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
  return check_manager_.GetLastCheckTimestamp().InMillisecondsSinceUnixEpoch();
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
        base::android::ConvertUTF8ToJavaString(
            env, credential.GetFirstSignonRealm()),
        url::GURLAndroid::FromNativeGURL(env, credential.GetURL()),
        base::android::ConvertUTF16ToJavaString(env, credential.username),
        base::android::ConvertUTF16ToJavaString(env, credential.display_origin),
        base::android::ConvertUTF16ToJavaString(env,
                                                credential.display_username),
        base::android::ConvertUTF16ToJavaString(env, credential.password),
        base::android::ConvertUTF8ToJavaString(env,
                                               credential.change_password_url),
        base::android::ConvertUTF8ToJavaString(env, credential.package_name),
        credential.GetLastLeakedOrPhishedTime().InMillisecondsSinceUnixEpoch(),
        credential.last_used_time.InMillisecondsSinceUnixEpoch(),
        IsOnlyLeaked(credential), IsOnlyPhished(credential));
  }
}

void PasswordCheckBridge::LaunchCheckupInAccount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& activity) {
  PasswordCheckupLauncherHelperImpl checkup_launcher;
  checkup_launcher.LaunchCheckupOnlineWithActivity(
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
      ConvertJavaObjectToCredential(env, credential),
      base::android::ConvertJavaStringToUTF8(new_password));
}

void PasswordCheckBridge::OnEditCredential(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential,
    const base::android::JavaParamRef<jobject>& context) {
  check_manager_.OnEditCredential(
      ConvertJavaObjectToCredential(env, credential), context);
}

void PasswordCheckBridge::RemoveCredential(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& credential) {
  check_manager_.RemoveCredential(
      ConvertJavaObjectToCredential(env, credential));
}

bool PasswordCheckBridge::HasAccountForRequest(JNIEnv* env) {
  return check_manager_.HasAccountForRequest();
}

void PasswordCheckBridge::Destroy(JNIEnv* env) {
  check_manager_.StopCheck();
  delete this;
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
