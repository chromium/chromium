// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/privacy_sandbox/android/jni_headers/TrackingProtectionBridge_jni.h"

static jint JNI_TrackingProtectionBridge_GetRequiredNotice(JNIEnv* env,
                                                           Profile* profile) {
  return static_cast<int>(
      TrackingProtectionOnboardingFactory::GetForProfile(profile)
          ->GetRequiredNotice());
}

static void JNI_TrackingProtectionBridge_NoticeRequested(JNIEnv* env,
                                                         Profile* profile,
                                                         jint noticeType) {
  return TrackingProtectionOnboardingFactory::GetForProfile(profile)
      ->NoticeRequested(
          static_cast<
              privacy_sandbox::TrackingProtectionOnboarding::NoticeType>(
              noticeType));
}

static void JNI_TrackingProtectionBridge_NoticeShown(JNIEnv* env,
                                                     Profile* profile,
                                                     jint noticeType) {
  return TrackingProtectionOnboardingFactory::GetForProfile(profile)
      ->NoticeShown(static_cast<
                    privacy_sandbox::TrackingProtectionOnboarding::NoticeType>(
          noticeType));
}

static void JNI_TrackingProtectionBridge_NoticeActionTaken(JNIEnv* env,
                                                           Profile* profile,
                                                           jint noticeType,
                                                           jint action) {
  return TrackingProtectionOnboardingFactory::GetForProfile(profile)
      ->NoticeActionTaken(
          static_cast<
              privacy_sandbox::TrackingProtectionOnboarding::NoticeType>(
              noticeType),
          static_cast<
              privacy_sandbox::TrackingProtectionOnboarding::NoticeAction>(
              action));
}

static jboolean JNI_TrackingProtectionBridge_IsOffboarded(JNIEnv* env,
                                                          Profile* profile) {
  return TrackingProtectionOnboardingFactory::GetForProfile(profile)
      ->IsOffboarded();
}
