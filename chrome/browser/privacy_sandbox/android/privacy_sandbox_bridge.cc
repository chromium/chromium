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
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries_impl.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
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
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

PrivacySandboxService* GetPrivacySandboxService(
    const base::android::JavaRef<jobject>& j_profile) {
  return PrivacySandboxServiceFactory::GetForProfile(
      Profile::FromJavaObject(j_profile));
}

std::vector<jni_zero::ScopedJavaLocalRef<jobject>> ToJavaTopicsArray(
    JNIEnv* env,
    const std::vector<privacy_sandbox::CanonicalTopic>& topics) {
  std::vector<ScopedJavaLocalRef<jobject>> j_topics;
  for (const auto& topic : topics) {
    j_topics.push_back(Java_PrivacySandboxBridge_createTopic(
        env, topic.topic_id().value(), topic.taxonomy_version(),
        ConvertUTF16ToJavaString(env, topic.GetLocalizedRepresentation()),
        ConvertUTF16ToJavaString(env, topic.GetLocalizedDescription())));
  }
  return j_topics;
}
}  // namespace

static jboolean JNI_PrivacySandboxBridge_IsPrivacySandboxRestricted(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  return GetPrivacySandboxService(j_profile)->IsPrivacySandboxRestricted();
}

static jboolean JNI_PrivacySandboxBridge_IsRestrictedNoticeEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  return GetPrivacySandboxService(j_profile)->IsRestrictedNoticeEnabled();
}

static std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
JNI_PrivacySandboxBridge_GetCurrentTopTopics(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  return ToJavaTopicsArray(
      env, GetPrivacySandboxService(j_profile)->GetCurrentTopTopics());
}

static std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
JNI_PrivacySandboxBridge_GetBlockedTopics(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  return ToJavaTopicsArray(
      env, GetPrivacySandboxService(j_profile)->GetBlockedTopics());
}

static std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
JNI_PrivacySandboxBridge_GetFirstLevelTopics(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  return ToJavaTopicsArray(
      env, GetPrivacySandboxService(j_profile)->GetFirstLevelTopics());
}

static std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
JNI_PrivacySandboxBridge_GetChildTopicsCurrentlyAssigned(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jint topic_id,
    jint taxonomy_version) {
  return ToJavaTopicsArray(
      env, GetPrivacySandboxService(j_profile)->GetChildTopicsCurrentlyAssigned(
               privacy_sandbox::CanonicalTopic(browsing_topics::Topic(topic_id),
                                               taxonomy_version)));
}

static void JNI_PrivacySandboxBridge_SetTopicAllowed(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jint topic_id,
    jint taxonomy_version,
    jboolean allowed) {
  GetPrivacySandboxService(j_profile)->SetTopicAllowed(
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(topic_id),
                                      taxonomy_version),
      allowed);
}

static void JNI_PrivacySandboxBridge_GetFledgeJoiningEtldPlusOneForDisplay(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_callback) {
  GetPrivacySandboxService(j_profile)->GetFledgeJoiningEtldPlusOneForDisplay(
      base::BindOnce(
          [](const base::android::JavaRef<jobject>& j_callback,
             std::vector<std::string> strings) {
            DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
            JNIEnv* env = base::android::AttachCurrentThread();
            base::android::RunObjectCallbackAndroid(
                j_callback, base::android::ToJavaArrayOfStrings(env, strings));
          },
          base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

static std::vector<std::string>
JNI_PrivacySandboxBridge_GetBlockedFledgeJoiningTopFramesForDisplay(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  return GetPrivacySandboxService(j_profile)
      ->GetBlockedFledgeJoiningTopFramesForDisplay();
}

static void JNI_PrivacySandboxBridge_SetFledgeJoiningAllowed(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& top_frame_etld_plus1,
    jboolean allowed) {
  GetPrivacySandboxService(j_profile)->SetFledgeJoiningAllowed(
      base::android::ConvertJavaStringToUTF8(top_frame_etld_plus1), allowed);
}

static jint JNI_PrivacySandboxBridge_GetRequiredPromptType(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jint surface_type) {
  // If the FRE is disabled, as it is in tests which must not be interrupted
  // with dialogs, do not attempt to show a dialog.
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch("disable-fre"))
    return static_cast<int>(PrivacySandboxService::PromptType::kNone);

  return static_cast<int>(
      GetPrivacySandboxService(j_profile)->GetRequiredPromptType(
          static_cast<PrivacySandboxService::SurfaceType>(surface_type)));
}

static void JNI_PrivacySandboxBridge_PromptActionOccurred(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jint action,
    jint surface_type) {
  GetPrivacySandboxService(j_profile)->PromptActionOccurred(
      static_cast<PrivacySandboxService::PromptAction>(action),
      static_cast<PrivacySandboxService::SurfaceType>(surface_type));
}

static jboolean JNI_PrivacySandboxBridge_IsFirstPartySetsDataAccessEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  return GetPrivacySandboxService(j_profile)
      ->IsFirstPartySetsDataAccessEnabled();
}

static jboolean JNI_PrivacySandboxBridge_IsFirstPartySetsDataAccessManaged(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  return GetPrivacySandboxService(j_profile)
      ->IsFirstPartySetsDataAccessManaged();
}

static void JNI_PrivacySandboxBridge_SetFirstPartySetsDataAccessEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jboolean enabled) {
  GetPrivacySandboxService(j_profile)->SetFirstPartySetsDataAccessEnabled(
      enabled);
}

static ScopedJavaLocalRef<jstring>
JNI_PrivacySandboxBridge_GetFirstPartySetOwner(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& memberOrigin) {
  auto rwsOwner = GetPrivacySandboxService(j_profile)->GetFirstPartySetOwner(
      GURL(base::android::ConvertJavaStringToUTF8(env, memberOrigin)));

  if (!rwsOwner.has_value()) {
    return nullptr;
  }

  return ConvertUTF8ToJavaString(env, rwsOwner->GetURL().host());
}

static jboolean JNI_PrivacySandboxBridge_IsPartOfManagedFirstPartySet(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& origin) {
  auto schemefulSite = net::SchemefulSite(
      GURL(base::android::ConvertJavaStringToUTF8(env, origin)));

  return GetPrivacySandboxService(j_profile)->IsPartOfManagedFirstPartySet(
      schemefulSite);
}

static void JNI_PrivacySandboxBridge_TopicsToggleChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jboolean new_value) {
  GetPrivacySandboxService(j_profile)->TopicsToggleChanged(new_value);
}

static void
JNI_PrivacySandboxBridge_SetAllPrivacySandboxAllowedForTesting(  // IN-TEST
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  PrivacySandboxSettingsFactory::GetForProfile(
      Profile::FromJavaObject(j_profile))
      ->SetAllPrivacySandboxAllowedForTesting();  // IN-TEST
}

static void JNI_PrivacySandboxBridge_RecordActivityType(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jint activity_type) {
  GetPrivacySandboxService(j_profile)->RecordActivityType(
      static_cast<PrivacySandboxService::PrivacySandboxStorageActivityType>(
          activity_type));
}

static jboolean
JNI_PrivacySandboxBridge_PrivacySandboxPrivacyGuideShouldShowAdTopicsCard(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  return GetPrivacySandboxService(j_profile)
      ->PrivacySandboxPrivacyGuideShouldShowAdTopicsCard();
}
