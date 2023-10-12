// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/android/jni_headers/TrackingProtectionBridge_jni.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"

namespace {
privacy_sandbox::TrackingProtectionOnboarding*
GetTrackingProtectionOnboardingService() {
  return TrackingProtectionOnboardingFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
}
}  // namespace

static jboolean JNI_TrackingProtectionBridge_ShouldShowOnboardingNotice(
    JNIEnv* env) {
  return GetTrackingProtectionOnboardingService()->ShouldShowOnboardingNotice();
}

static void JNI_TrackingProtectionBridge_NoticeShown(JNIEnv* env) {
  return GetTrackingProtectionOnboardingService()->NoticeShown();
}

static void JNI_TrackingProtectionBridge_NoticeActionTaken(JNIEnv* env,
                                                           jint action) {
  return GetTrackingProtectionOnboardingService()->NoticeActionTaken(
      static_cast<privacy_sandbox::TrackingProtectionOnboarding::NoticeAction>(
          action));
}
