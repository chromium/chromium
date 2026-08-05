// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/privacy_sandbox/android/jni_headers/PrivacySandboxBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {

PrivacySandboxService* GetPrivacySandboxService(
    const base::android::JavaRef<jobject>& j_profile) {
  return PrivacySandboxServiceFactory::GetForProfile(
      Profile::FromJavaObject(j_profile));
}
}  // namespace

static bool JNI_PrivacySandboxBridge_IsRelatedWebsiteSetsDataAccessEnabled(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile) {
  return GetPrivacySandboxService(j_profile)
      ->IsRelatedWebsiteSetsDataAccessEnabled();
}

static bool JNI_PrivacySandboxBridge_IsRelatedWebsiteSetsDataAccessManaged(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile) {
  return GetPrivacySandboxService(j_profile)
      ->IsRelatedWebsiteSetsDataAccessManaged();
}

static void JNI_PrivacySandboxBridge_SetRelatedWebsiteSetsDataAccessEnabled(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile,
    bool enabled) {
  GetPrivacySandboxService(j_profile)->SetRelatedWebsiteSetsDataAccessEnabled(
      enabled);
}

static ScopedJavaLocalRef<jstring>
JNI_PrivacySandboxBridge_GetRelatedWebsiteSetOwner(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile,
    const JavaRef<jstring>& memberOrigin) {
  auto rwsOwner =
      GetPrivacySandboxService(j_profile)->GetRelatedWebsiteSetOwner(
          GURL(base::android::ConvertJavaStringToUTF8(env, memberOrigin)));

  if (!rwsOwner.has_value()) {
    return nullptr;
  }

  return ConvertUTF8ToJavaString(env, rwsOwner->GetURL().GetHost());
}

static bool JNI_PrivacySandboxBridge_IsPartOfManagedRelatedWebsiteSet(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile,
    const JavaRef<jstring>& origin) {
  auto schemefulSite = net::SchemefulSite(
      GURL(base::android::ConvertJavaStringToUTF8(env, origin)));

  return GetPrivacySandboxService(j_profile)->IsPartOfManagedRelatedWebsiteSet(
      schemefulSite);
}

DEFINE_JNI(PrivacySandboxBridge)
