// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_synced_theme_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "chrome/browser/ntp_customization/ntp_android_custom_background_service.h"
#include "chrome/browser/ntp_customization/ntp_android_custom_background_service_factory.h"
#include "chrome/browser/ntp_customization/ntp_customization_utils.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ntp_customization/jni_headers/NtpSyncedThemeBridge_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

static int64_t JNI_NtpSyncedThemeBridge_Init(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile,
    const JavaRef<jobject>& j_java_obj) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  NtpSyncedThemeBridge* ntp_customization_theme_bridge =
      new NtpSyncedThemeBridge(env, profile, j_java_obj);
  return reinterpret_cast<intptr_t>(ntp_customization_theme_bridge);
}

NtpSyncedThemeBridge::NtpSyncedThemeBridge(JNIEnv* env,
                                           Profile* profile,
                                           const JavaRef<jobject>& j_java_obj)
    : profile_(profile),
      ntp_custom_background_service_(
          NtpAndroidCustomBackgroundServiceFactory::GetForProfile(profile)),
      j_java_obj_(env, j_java_obj) {
  CHECK(ntp_custom_background_service_);
  ntp_custom_background_service_->SetSyncedThemeBridge(this);
}

void NtpSyncedThemeBridge::Destroy(JNIEnv* env) {
  if (ntp_custom_background_service_) {
    ntp_custom_background_service_->SetSyncedThemeBridge(nullptr);
  }
  delete this;
}

void NtpSyncedThemeBridge::DisconnectCustomBackgroundService() {
  ntp_custom_background_service_ = nullptr;
}

NtpSyncedThemeBridge::NtpSyncedThemeBridge() = default;
NtpSyncedThemeBridge::~NtpSyncedThemeBridge() = default;

void NtpSyncedThemeBridge::FetchNextThemeCollectionImage(JNIEnv* env) {
  if (!ntp_custom_background_service_) {
    return;
  }
  ntp_custom_background_service_->RefreshBackgroundIfNeeded();
}

ScopedJavaLocalRef<jobject> NtpSyncedThemeBridge::GetCustomBackgroundInfo(
    JNIEnv* env) {
  if (!ntp_custom_background_service_) {
    return nullptr;
  }
  std::optional<CustomBackground> background =
      ntp_custom_background_service_->GetCustomBackground();
  if (!background.has_value()) {
    return nullptr;
  }

  return Java_NtpSyncedThemeBridge_createCustomBackgroundInfo(
      env, background->custom_background_url, background->collection_id,
      background->is_uploaded_image, background->daily_refresh_enabled,
      ntp_customization::GetCustomBackgroundAttribution(*background));
}

bool NtpSyncedThemeBridge::IsProcessingSyncUpdate(JNIEnv* env) {
  if (!ntp_custom_background_service_) {
    return false;
  }
  return ntp_custom_background_service_->IsProcessingSyncUpdate();
}

void NtpSyncedThemeBridge::SetChromeColor(JNIEnv* env, int color_id) {
  if (!ntp_custom_background_service_) {
    return;
  }
  ntp_custom_background_service_->SetChromeColor(color_id);
}

void NtpSyncedThemeBridge::ResetCustomBackgroundInfo(JNIEnv* env) {
  if (!ntp_custom_background_service_) {
    return;
  }
  ntp_custom_background_service_->ResetCustomBackgroundInfo();
}

void NtpSyncedThemeBridge::SelectLocalBackgroundImage(JNIEnv* env) {
  if (!ntp_custom_background_service_) {
    return;
  }
  ntp_custom_background_service_->SelectLocalBackgroundImage(base::FilePath());
}

void NtpSyncedThemeBridge::UpdateCustomBackgroundPrefsWithColor(
    const GURL& url,
    int32_t primary_color) {
  if (!ntp_custom_background_service_) {
    return;
  }

  ntp_custom_background_service_->UpdateCustomBackgroundPrefsWithColor(
      url, static_cast<SkColor>(primary_color));
}

void NtpSyncedThemeBridge::OnChromeColorSynced(int color_id) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_NtpSyncedThemeBridge_onChromeColorSynced(env, j_java_obj_, color_id);
}

void NtpSyncedThemeBridge::OnDefaultThemeSynced() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_NtpSyncedThemeBridge_onDefaultThemeSynced(env, j_java_obj_);
}

void NtpSyncedThemeBridge::OnCustomBackgroundImageUpdated() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_NtpSyncedThemeBridge_onCustomBackgroundImageUpdated(env, j_java_obj_);
}

DEFINE_JNI(NtpSyncedThemeBridge)
