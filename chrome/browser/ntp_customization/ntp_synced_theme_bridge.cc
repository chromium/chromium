// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_synced_theme_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "chrome/browser/ntp_customization/jni_headers/NtpSyncedThemeBridge_jni.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "url/android/gurl_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static jlong JNI_NtpSyncedThemeBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_java_obj) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  NtpSyncedThemeBridge* ntp_customization_theme_bridge =
      new NtpSyncedThemeBridge(env, profile, j_java_obj);
  return reinterpret_cast<intptr_t>(ntp_customization_theme_bridge);
}

NtpSyncedThemeBridge::NtpSyncedThemeBridge(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& j_java_obj)
    : profile_(profile),
      ntp_custom_background_service_(
          NtpCustomBackgroundServiceFactory::GetForProfile(profile)),
      j_java_obj_(env, j_java_obj) {
  CHECK(ntp_custom_background_service_);
  ntp_custom_background_service_->AddObserver(this);
}

void NtpSyncedThemeBridge::Destroy(JNIEnv* env) {
  if (ntp_custom_background_service_) {
    ntp_custom_background_service_->RemoveObserver(this);
  }
  delete this;
}

NtpSyncedThemeBridge::~NtpSyncedThemeBridge() = default;

ScopedJavaLocalRef<jobject> NtpSyncedThemeBridge::GetCustomBackgroundInfo(
    JNIEnv* env) {
  std::optional<CustomBackground> background =
      ntp_custom_background_service_->GetCustomBackground();
  if (!background.has_value()) {
    return nullptr;
  }

  ScopedJavaLocalRef<jobject> j_url =
      url::GURLAndroid::FromNativeGURL(env, background->custom_background_url);
  ScopedJavaLocalRef<jstring> j_collection_id =
      base::android::ConvertUTF8ToJavaString(env, background->collection_id);

  return Java_NtpSyncedThemeBridge_createCustomBackgroundInfo(
      env, j_url, j_collection_id, background->is_uploaded_image,
      background->daily_refresh_enabled);
}

void NtpSyncedThemeBridge::OnCustomBackgroundImageUpdated() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NtpSyncedThemeBridge_onCustomBackgroundImageUpdated(env, j_java_obj_);
}

DEFINE_JNI(NtpSyncedThemeBridge)
