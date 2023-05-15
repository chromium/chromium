// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/android/touch_to_fill_view_impl.h"

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/touch_to_fill/android/jni_headers/Credential_jni.h"
#include "chrome/browser/touch_to_fill/android/jni_headers/TouchToFillBridge_jni.h"
#include "chrome/browser/touch_to_fill/android/jni_headers/WebAuthnCredential_jni.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"  // nogncheck
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using password_manager::PasskeyCredential;
using password_manager::UiCredential;

namespace {

UiCredential ConvertJavaCredential(JNIEnv* env,
                                   const JavaParamRef<jobject>& credential) {
  return UiCredential(
      ConvertJavaStringToUTF16(env,
                               Java_Credential_getUsername(env, credential)),
      ConvertJavaStringToUTF16(env,
                               Java_Credential_getPassword(env, credential)),
      url::Origin::Create(GURL(ConvertJavaStringToUTF8(
          env, Java_Credential_getOriginUrl(env, credential)))),
      UiCredential::IsPublicSuffixMatch(
          Java_Credential_isPublicSuffixMatch(env, credential)),
      UiCredential::IsAffiliationBasedMatch(
          Java_Credential_isAffiliationBasedMatch(env, credential)),
      base::Time::FromJavaTime(
          Java_Credential_lastUsedMsSinceEpoch(env, credential)));
}

PasskeyCredential ConvertJavaWebAuthnCredential(
    JNIEnv* env,
    const JavaParamRef<jobject>& credential) {
  std::vector<uint8_t> credential_id;
  base::android::JavaByteArrayToByteVector(
      env, Java_WebAuthnCredential_getCredentialId(env, credential),
      &credential_id);

  std::vector<uint8_t> user_id;
  base::android::JavaByteArrayToByteVector(
      env, Java_WebAuthnCredential_getUserId(env, credential), &user_id);

  return PasskeyCredential(
      PasskeyCredential::Source::kAndroidPhone,
      ConvertJavaStringToUTF8(Java_WebAuthnCredential_getRpId(env, credential)),
      std::move(credential_id), std::move(user_id),
      ConvertJavaStringToUTF8(
          Java_WebAuthnCredential_getUsername(env, credential)));
}

}  // namespace

TouchToFillViewImpl::TouchToFillViewImpl(TouchToFillController* controller)
    : controller_(controller) {}

TouchToFillViewImpl::~TouchToFillViewImpl() {
  if (java_object_internal_) {
    // Don't create an object just for destruction.
    Java_TouchToFillBridge_destroy(AttachCurrentThread(),
                                   java_object_internal_);
  }
}

void TouchToFillViewImpl::Show(
    const GURL& url,
    IsOriginSecure is_origin_secure,
    base::span<const password_manager::UiCredential> credentials,
    base::span<const PasskeyCredential> passkey_credentials,
    bool trigger_submission,
    bool can_manage_passwords_when_passkeys_present) {
  if (!RecreateJavaObject()) {
    // It's possible that the constructor cannot access the bottom sheet clank
    // component. That case may be temporary but we can't let users in a waiting
    // state so report that TouchToFill is dismissed in order to show the normal
    // Android keyboard (plus keyboard accessory) instead.
    controller_->OnDismiss();
    return;
  }
  // Serialize the |credentials| span into a Java array and instruct the bridge
  // to show it together with |url| to the user.
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> credential_array =
      Java_TouchToFillBridge_createCredentialArray(env, credentials.size());
  for (size_t i = 0; i < credentials.size(); ++i) {
    const password_manager::UiCredential& credential = credentials[i];
    Java_TouchToFillBridge_insertCredential(
        env, credential_array, i,
        ConvertUTF16ToJavaString(env, credential.username()),
        ConvertUTF16ToJavaString(env, credential.password()),
        ConvertUTF16ToJavaString(env, GetDisplayUsername(credential)),
        ConvertUTF8ToJavaString(env, credential.origin().Serialize()),
        credential.is_public_suffix_match().value(),
        credential.is_affiliation_based_match().value(),
        credential.last_used().ToJavaTime());
  }

  base::android::ScopedJavaLocalRef<jobjectArray> passkey_array =
      Java_TouchToFillBridge_createWebAuthnCredentialArray(
          env, passkey_credentials.size());
  for (size_t i = 0; i < passkey_credentials.size(); ++i) {
    const PasskeyCredential& credential = passkey_credentials[i];
    Java_TouchToFillBridge_insertWebAuthnCredential(
        env, passkey_array, i, ConvertUTF8ToJavaString(env, credential.rp_id()),
        base::android::ToJavaByteArray(env, credential.credential_id()),
        base::android::ToJavaByteArray(env, credential.user_id()),
        ConvertUTF16ToJavaString(
            env, password_manager::ToUsernameString(credential.username())));
  }

  Java_TouchToFillBridge_showCredentials(
      env, java_object_internal_, url::GURLAndroid::FromNativeGURL(env, url),
      is_origin_secure.value(), passkey_array, credential_array,
      trigger_submission, !can_manage_passwords_when_passkeys_present);
}

void TouchToFillViewImpl::OnCredentialSelected(const UiCredential& credential) {
  controller_->OnCredentialSelected(credential);
}

void TouchToFillViewImpl::OnDismiss() {
  controller_->OnDismiss();
}

void TouchToFillViewImpl::OnCredentialSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& credential) {
  OnCredentialSelected(ConvertJavaCredential(env, credential));
}

void TouchToFillViewImpl::OnWebAuthnCredentialSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& credential) {
  controller_->OnPasskeyCredentialSelected(
      ConvertJavaWebAuthnCredential(env, credential));
}

void TouchToFillViewImpl::OnManagePasswordsSelected(JNIEnv* env,
                                                    jboolean passkeys_shown) {
  controller_->OnManagePasswordsSelected(passkeys_shown);
}

void TouchToFillViewImpl::OnDismiss(JNIEnv* env) {
  OnDismiss();
}

bool TouchToFillViewImpl::RecreateJavaObject() {
  if (controller_->GetNativeView() == nullptr ||
      controller_->GetNativeView()->GetWindowAndroid() == nullptr) {
    return false;  // No window attached (yet or anymore).
  }
  if (java_object_internal_) {
    Java_TouchToFillBridge_destroy(AttachCurrentThread(),
                                   java_object_internal_);
  }
  java_object_internal_ = Java_TouchToFillBridge_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      controller_->GetNativeView()->GetWindowAndroid()->GetJavaObject());
  return !!java_object_internal_;
}
