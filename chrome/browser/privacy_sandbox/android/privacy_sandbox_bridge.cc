// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/android/jni_headers/PrivacySandboxBridge_jni.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {
// TODO(crbug.com/1286276): Remove this fake implementation and call
// PrivacySandboxService.
std::set<std::string>* GetCurrentTopics() {
  static base::NoDestructor<std::set<std::string>> current_topics(
      {"Generated sample data", "More made up data"});
  return current_topics.get();
}
std::set<std::string>* GetBlockedTopics() {
  static base::NoDestructor<std::set<std::string>> blocked_topics({});
  return blocked_topics.get();
}
}  // namespace

static jboolean JNI_PrivacySandboxBridge_IsPrivacySandboxEnabled(JNIEnv* env) {
  return PrivacySandboxServiceFactory::GetForProfile(
             ProfileManager::GetActiveUserProfile())
      ->IsPrivacySandboxEnabled();
}

static jboolean JNI_PrivacySandboxBridge_IsPrivacySandboxManaged(JNIEnv* env) {
  return PrivacySandboxServiceFactory::GetForProfile(
             ProfileManager::GetActiveUserProfile())
      ->IsPrivacySandboxManaged();
}

static void JNI_PrivacySandboxBridge_SetPrivacySandboxEnabled(
    JNIEnv* env,
    jboolean enabled) {
  PrivacySandboxSettingsFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->SetPrivacySandboxEnabled(enabled);
}

static jboolean JNI_PrivacySandboxBridge_IsFlocEnabled(JNIEnv* env) {
  return PrivacySandboxServiceFactory::GetForProfile(
             ProfileManager::GetActiveUserProfile())
      ->IsFlocPrefEnabled();
}

static void JNI_PrivacySandboxBridge_SetFlocEnabled(JNIEnv* env,
                                                    jboolean enabled) {
  PrivacySandboxServiceFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->SetFlocPrefEnabled(enabled);
}

static jboolean JNI_PrivacySandboxBridge_IsFlocIdResettable(JNIEnv* env) {
  return PrivacySandboxServiceFactory::GetForProfile(
             ProfileManager::GetActiveUserProfile())
      ->IsFlocIdResettable();
}

static void JNI_PrivacySandboxBridge_ResetFlocId(JNIEnv* env) {
  PrivacySandboxServiceFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->ResetFlocId(/*user_initiated=*/true);
}

static ScopedJavaLocalRef<jstring> JNI_PrivacySandboxBridge_GetFlocStatusString(
    JNIEnv* env) {
  return ConvertUTF16ToJavaString(env,
                                  PrivacySandboxServiceFactory::GetForProfile(
                                      ProfileManager::GetActiveUserProfile())
                                      ->GetFlocStatusForDisplay());
}

static ScopedJavaLocalRef<jstring> JNI_PrivacySandboxBridge_GetFlocGroupString(
    JNIEnv* env) {
  return ConvertUTF16ToJavaString(env,
                                  PrivacySandboxServiceFactory::GetForProfile(
                                      ProfileManager::GetActiveUserProfile())
                                      ->GetFlocIdForDisplay());
}

static ScopedJavaLocalRef<jstring> JNI_PrivacySandboxBridge_GetFlocUpdateString(
    JNIEnv* env) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return ConvertUTF16ToJavaString(
      env, PrivacySandboxServiceFactory::GetForProfile(profile)
               ->GetFlocIdNextUpdateForDisplay(base::Time::Now()));
}

static ScopedJavaLocalRef<jstring>
JNI_PrivacySandboxBridge_GetFlocDescriptionString(JNIEnv* env) {
  return ConvertUTF16ToJavaString(env,
                                  PrivacySandboxServiceFactory::GetForProfile(
                                      ProfileManager::GetActiveUserProfile())
                                      ->GetFlocDescriptionForDisplay());
}

static ScopedJavaLocalRef<jstring>
JNI_PrivacySandboxBridge_GetFlocResetExplanationString(JNIEnv* env) {
  return ConvertUTF16ToJavaString(env,
                                  PrivacySandboxServiceFactory::GetForProfile(
                                      ProfileManager::GetActiveUserProfile())
                                      ->GetFlocResetExplanationForDisplay());
}

// TODO(crbug.com/1286276): Remove this fake implementation and call
// PrivacySandboxService.
static ScopedJavaLocalRef<jobjectArray>
JNI_PrivacySandboxBridge_GetCurrentTopTopics(JNIEnv* env) {
  return base::android::ToJavaArrayOfStrings(
      env, std::vector<std::string>(GetCurrentTopics()->begin(),
                                    GetCurrentTopics()->end()));
}

static ScopedJavaLocalRef<jobjectArray>
JNI_PrivacySandboxBridge_GetBlockedTopics(JNIEnv* env) {
  return base::android::ToJavaArrayOfStrings(
      env, std::vector<std::string>(GetBlockedTopics()->begin(),
                                    GetBlockedTopics()->end()));
}

static void JNI_PrivacySandboxBridge_SetTopicAllowed(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_topic,
    jboolean allowed) {
  std::string topic = base::android::ConvertJavaStringToUTF8(j_topic);
  if (allowed) {
    GetCurrentTopics()->insert(topic);
    GetBlockedTopics()->erase(topic);
  } else {
    GetCurrentTopics()->erase(topic);
    GetBlockedTopics()->insert(topic);
  }
}

static jint JNI_PrivacySandboxBridge_GetRequiredDialogType(JNIEnv* env) {
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
