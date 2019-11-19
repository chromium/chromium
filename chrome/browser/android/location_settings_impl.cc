// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/location_settings_impl.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/LocationSettings_jni.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;

using LocationSettingsDialogOutcomeCallback =
    LocationSettings::LocationSettingsDialogOutcomeCallback;

LocationSettingsImpl::LocationSettingsImpl() {}

LocationSettingsImpl::~LocationSettingsImpl() {}

bool LocationSettingsImpl::HasAndroidLocationPermission() {
  JNIEnv* env = AttachCurrentThread();
  return Java_LocationSettings_hasAndroidLocationPermission(env);
}

bool LocationSettingsImpl::CanPromptForAndroidLocationPermission(
    content::WebContents* web_contents) {
  JNIEnv* env = AttachCurrentThread();
  return Java_LocationSettings_canPromptForAndroidLocationPermission(
      env, web_contents->GetJavaWebContents());
}

bool LocationSettingsImpl::IsSystemLocationSettingEnabled() {
  JNIEnv* env = AttachCurrentThread();
  return Java_LocationSettings_isSystemLocationSettingEnabled(env);
}

bool LocationSettingsImpl::CanPromptToEnableSystemLocationSetting() {
  JNIEnv* env = AttachCurrentThread();
  return Java_LocationSettings_canPromptToEnableSystemLocationSetting(env);
}

void LocationSettingsImpl::PromptToEnableSystemLocationSetting(
    const LocationSettingsDialogContext prompt_context,
    content::WebContents* web_contents,
    LocationSettingsDialogOutcomeCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  // Transfers the ownership of the callback to the Java callback. The Java
  // callback is guaranteed to be called unless the user never replies to the
  // dialog, and the callback pointer will be destroyed in
  // OnLocationSettingsDialogOutcome.
  auto* callback_ptr =
      new LocationSettingsDialogOutcomeCallback(std::move(callback));
  Java_LocationSettings_promptToEnableSystemLocationSetting(
      env, prompt_context, web_contents->GetJavaWebContents(),
      reinterpret_cast<jlong>(callback_ptr));
}

static void JNI_LocationSettings_OnLocationSettingsDialogOutcome(
    JNIEnv* env,
    jlong callback_ptr,
    int result) {
  auto* callback =
      reinterpret_cast<LocationSettingsDialogOutcomeCallback*>(callback_ptr);
  std::move(*callback).Run(static_cast<LocationSettingsDialogOutcome>(result));
  // Destroy the callback whose ownership was transferred in
  // PromptToEnableSystemLocationSetting.
  delete callback;
}
