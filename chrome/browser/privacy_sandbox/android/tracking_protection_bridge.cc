// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_reminder_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/privacy_sandbox/tracking_protection_reminder_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/privacy_sandbox/android/jni_headers/TrackingProtectionBridge_jni.h"

static jint JNI_TrackingProtectionBridge_GetRequiredNotice(JNIEnv* env,
                                                           Profile* profile,
                                                           jint surface) {
  return static_cast<int>(
      TrackingProtectionOnboardingFactory::GetForProfile(profile)
          ->GetRequiredNotice(
              static_cast<
                  privacy_sandbox::TrackingProtectionOnboarding::SurfaceType>(
                  surface)));
}

static void JNI_TrackingProtectionBridge_NoticeShown(JNIEnv* env,
                                                     Profile* profile,
                                                     jint surface,
                                                     jint noticeType) {
  return TrackingProtectionOnboardingFactory::GetForProfile(profile)
      ->NoticeShown(
          static_cast<
              privacy_sandbox::TrackingProtectionOnboarding::SurfaceType>(
              surface),
          static_cast<
              privacy_sandbox::TrackingProtectionOnboarding::NoticeType>(
              noticeType));
}

static void JNI_TrackingProtectionBridge_NoticeActionTaken(JNIEnv* env,
                                                           Profile* profile,
                                                           jint surface,
                                                           jint noticeType,
                                                           jint action) {
  return TrackingProtectionOnboardingFactory::GetForProfile(profile)
      ->NoticeActionTaken(
          static_cast<
              privacy_sandbox::TrackingProtectionOnboarding::SurfaceType>(
              surface),
          static_cast<
              privacy_sandbox::TrackingProtectionOnboarding::NoticeType>(
              noticeType),
          static_cast<
              privacy_sandbox::TrackingProtectionOnboarding::NoticeAction>(
              action));
}

static jboolean JNI_TrackingProtectionBridge_IsOffboarded(JNIEnv* env,
                                                          Profile* profile) {
  // Always returning false for now. This is to be removed as part of
  // crbug/344565466.
  return false;
}

static jboolean JNI_TrackingProtectionBridge_ShouldRunUILogic(JNIEnv* env,
                                                              Profile* profile,
                                                              jint surface) {
  return TrackingProtectionOnboardingFactory::GetForProfile(profile)
      ->ShouldRunUILogic(
          static_cast<
              privacy_sandbox::TrackingProtectionOnboarding::SurfaceType>(
              surface));
}

static jint JNI_TrackingProtectionBridge_GetReminderType(JNIEnv* env,
                                                         Profile* profile) {
  return static_cast<int>(
      TrackingProtectionReminderFactory::GetForProfile(profile)
          ->GetReminderType());
}

static void JNI_TrackingProtectionBridge_OnReminderExperienced(
    JNIEnv* env,
    Profile* profile) {
  TrackingProtectionReminderFactory::GetForProfile(profile)
      ->OnReminderExperienced();
}
