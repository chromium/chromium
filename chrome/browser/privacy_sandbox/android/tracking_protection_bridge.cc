// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/android/jni_headers/TrackingProtectionBridge_jni.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"

namespace {
privacy_sandbox::TrackingProtectionOnboarding*
GetTrackingProtectionOnboardingService(
    const base::android::JavaRef<jobject>& j_profile) {
  return TrackingProtectionOnboardingFactory::GetForProfile(
      ProfileAndroid::FromProfileAndroid(j_profile));
}
}  // namespace

static jint JNI_TrackingProtectionBridge_GetRequiredNotice(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile) {
  return static_cast<int>(
      GetTrackingProtectionOnboardingService(j_profile)->GetRequiredNotice());
}

static void JNI_TrackingProtectionBridge_NoticeRequested(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    jint noticeType) {
  return GetTrackingProtectionOnboardingService(j_profile)->NoticeRequested(
      static_cast<privacy_sandbox::TrackingProtectionOnboarding::NoticeType>(
          noticeType));
}

static void JNI_TrackingProtectionBridge_NoticeShown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    jint noticeType) {
  return GetTrackingProtectionOnboardingService(j_profile)->NoticeShown(
      static_cast<privacy_sandbox::TrackingProtectionOnboarding::NoticeType>(
          noticeType));
}

static void JNI_TrackingProtectionBridge_NoticeActionTaken(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    jint noticeType,
    jint action) {
  return GetTrackingProtectionOnboardingService(j_profile)->NoticeActionTaken(
      static_cast<privacy_sandbox::TrackingProtectionOnboarding::NoticeType>(
          noticeType),
      static_cast<privacy_sandbox::TrackingProtectionOnboarding::NoticeAction>(
          action));
}

static jboolean JNI_TrackingProtectionBridge_IsOffboarded(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile) {
  return GetTrackingProtectionOnboardingService(j_profile)->IsOffboarded();
}
