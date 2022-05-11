// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/android/jni_headers/PrivacySandboxBridge_jni.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {
const char TOPICS_JAVA_CLASS[] =
    "org/chromium/chrome/browser/privacy_sandbox/Topic";

PrivacySandboxService* GetPrivacySandboxService() {
  return PrivacySandboxServiceFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
}

ScopedJavaLocalRef<jobjectArray> ToJavaTopicsArray(
    JNIEnv* env,
    const std::vector<privacy_sandbox::CanonicalTopic>& topics) {
  std::vector<ScopedJavaLocalRef<jobject>> j_topics;
  for (const auto& topic : topics) {
    j_topics.push_back(Java_PrivacySandboxBridge_createTopic(
        env, topic.topic_id().value(), topic.taxonomy_version(),
        ConvertUTF16ToJavaString(env, topic.GetLocalizedRepresentation())));
  }
  return base::android::ToJavaArrayOfObjects(
      env, base::android::GetClass(env, TOPICS_JAVA_CLASS), j_topics);
}
}  // namespace

static jboolean JNI_PrivacySandboxBridge_IsPrivacySandboxEnabled(JNIEnv* env) {
  return GetPrivacySandboxService()->IsPrivacySandboxEnabled();
}

static jboolean JNI_PrivacySandboxBridge_IsPrivacySandboxManaged(JNIEnv* env) {
  return GetPrivacySandboxService()->IsPrivacySandboxManaged();
}

static jboolean JNI_PrivacySandboxBridge_IsPrivacySandboxRestricted(
    JNIEnv* env) {
  return GetPrivacySandboxService()->IsPrivacySandboxRestricted();
}

static void JNI_PrivacySandboxBridge_SetPrivacySandboxEnabled(
    JNIEnv* env,
    jboolean enabled) {
  GetPrivacySandboxService()->SetPrivacySandboxEnabled(enabled);
}

static jboolean JNI_PrivacySandboxBridge_IsFlocEnabled(JNIEnv* env) {
  return GetPrivacySandboxService()->IsFlocPrefEnabled();
}

static void JNI_PrivacySandboxBridge_SetFlocEnabled(JNIEnv* env,
                                                    jboolean enabled) {
  GetPrivacySandboxService()->SetFlocPrefEnabled(enabled);
}

static jboolean JNI_PrivacySandboxBridge_IsFlocIdResettable(JNIEnv* env) {
  return GetPrivacySandboxService()->IsFlocIdResettable();
}

static void JNI_PrivacySandboxBridge_ResetFlocId(JNIEnv* env) {
  GetPrivacySandboxService()->ResetFlocId(/*user_initiated=*/true);
}

static ScopedJavaLocalRef<jstring> JNI_PrivacySandboxBridge_GetFlocStatusString(
    JNIEnv* env) {
  return ConvertUTF16ToJavaString(
      env, GetPrivacySandboxService()->GetFlocStatusForDisplay());
}

static ScopedJavaLocalRef<jstring> JNI_PrivacySandboxBridge_GetFlocGroupString(
    JNIEnv* env) {
  return ConvertUTF16ToJavaString(
      env, GetPrivacySandboxService()->GetFlocIdForDisplay());
}

static ScopedJavaLocalRef<jstring> JNI_PrivacySandboxBridge_GetFlocUpdateString(
    JNIEnv* env) {
  return ConvertUTF16ToJavaString(
      env, GetPrivacySandboxService()->GetFlocIdNextUpdateForDisplay(
               base::Time::Now()));
}

static ScopedJavaLocalRef<jstring>
JNI_PrivacySandboxBridge_GetFlocDescriptionString(JNIEnv* env) {
  return ConvertUTF16ToJavaString(
      env, GetPrivacySandboxService()->GetFlocDescriptionForDisplay());
}

static ScopedJavaLocalRef<jstring>
JNI_PrivacySandboxBridge_GetFlocResetExplanationString(JNIEnv* env) {
  return ConvertUTF16ToJavaString(
      env, GetPrivacySandboxService()->GetFlocResetExplanationForDisplay());
}

static ScopedJavaLocalRef<jobjectArray>
JNI_PrivacySandboxBridge_GetCurrentTopTopics(JNIEnv* env) {
  return ToJavaTopicsArray(env,
                           GetPrivacySandboxService()->GetCurrentTopTopics());
}

static ScopedJavaLocalRef<jobjectArray>
JNI_PrivacySandboxBridge_GetBlockedTopics(JNIEnv* env) {
  return ToJavaTopicsArray(env, GetPrivacySandboxService()->GetBlockedTopics());
}

static void JNI_PrivacySandboxBridge_SetTopicAllowed(JNIEnv* env,
                                                     jint topic_id,
                                                     jint taxonomy_version,
                                                     jboolean allowed) {
  GetPrivacySandboxService()->SetTopicAllowed(
      privacy_sandbox::CanonicalTopic(browsing_topics::Topic(topic_id),
                                      taxonomy_version),
      allowed);
}

static jint JNI_PrivacySandboxBridge_GetRequiredDialogType(JNIEnv* env) {
  // If the FRE is disabled, as it is in tests which must not be interrupted
  // with dialogs, do not attempt to show a dialog.
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch("disable-fre"))
    return static_cast<int>(PrivacySandboxService::DialogType::kNone);

  return static_cast<int>(PrivacySandboxServiceFactory::GetForProfile(
                              ProfileManager::GetActiveUserProfile())
                              ->GetRequiredDialogType());
}

static void JNI_PrivacySandboxBridge_DialogActionOccurred(JNIEnv* env,
                                                          jint action) {
  PrivacySandboxServiceFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->DialogActionOccurred(
          static_cast<PrivacySandboxService::DialogAction>(action));
}
