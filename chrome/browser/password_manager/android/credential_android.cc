// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/credential_android.h"

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/Credential_jni.h"

base::android::ScopedJavaLocalRef<jobject> CreateNativeCredential(
    JNIEnv* env,
    const password_manager::PasswordForm& password_form,
    int position) {
  using base::android::ConvertUTF16ToJavaString;
  using base::android::ConvertUTF8ToJavaString;
  // Origin URL should be shown for the PSL, affiliated and grouped matches.
  std::string origin_url =
      password_manager_util::GetMatchType(password_form) ==
              password_manager_util::GetLoginMatchType::kExact
          ? std::string()
          : password_form.url.DeprecatedGetOriginAsURL().spec();
  std::string federation =
      password_form.IsFederatedCredential()
          ? l10n_util::GetStringFUTF8(
                IDS_PASSWORDS_VIA_FEDERATION,
                base::ASCIIToUTF16(password_form.federation_origin.host()))
          : std::string();
  return Java_Credential_createCredential(
      env, ConvertUTF16ToJavaString(env, password_form.username_value),
      ConvertUTF16ToJavaString(env, password_form.display_name),
      ConvertUTF8ToJavaString(env, origin_url),
      ConvertUTF8ToJavaString(env, federation), position);
}

base::android::ScopedJavaLocalRef<jobjectArray> CreateNativeCredentialArray(
    JNIEnv* env,
    size_t size) {
  return Java_Credential_createCredentialArray(env, static_cast<int>(size));
}
