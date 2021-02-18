// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/android/jni_headers/PrivacySandboxBridge_jni.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile_manager.h"

static jboolean JNI_PrivacySandboxBridge_IsPrivacySandboxSettingsFunctional(
    JNIEnv* env) {
  return PrivacySandboxSettingsFactory::GetForProfile(
             ProfileManager::GetActiveUserProfile())
      ->PrivacySandboxSettingsFunctional();
}

static jboolean JNI_PrivacySandboxBridge_IsPrivacySandboxEnabled(JNIEnv* env) {
  return PrivacySandboxSettingsFactory::GetForProfile(
             ProfileManager::GetActiveUserProfile())
      ->IsPrivacySandboxEnabled();
}

static jboolean JNI_PrivacySandboxBridge_IsPrivacySandboxManaged(JNIEnv* env) {
  return PrivacySandboxSettingsFactory::GetForProfile(
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
