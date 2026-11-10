// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_synced_theme_bridge.h"

#include <optional>
#include <string>

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "chrome/browser/ntp_customization/ntp_android_custom_background_service.h"
#include "chrome/browser/ntp_customization/ntp_android_custom_background_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/themes/ntp_custom_background_service_base.h"
#include "third_party/jni_zero/jni_zero.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ntp_customization/jni_headers/NtpSyncedThemeBridge_jni.h"

using base::android::ScopedJavaLocalRef;

namespace {

// Returns the service the Java bridge of `profile` talks to. The service lives
// as long as the profile does, so the lookup only yields null for an
// off-the-record profile (the factory is kOriginalOnly), which Java must never
// pass.
NtpAndroidCustomBackgroundService* GetService(Profile* profile) {
  NtpAndroidCustomBackgroundService* service =
      NtpAndroidCustomBackgroundServiceFactory::GetForProfile(profile);
  CHECK(service);
  return service;
}

}  // namespace

NtpSyncedThemeBridge::NtpSyncedThemeBridge(Profile* profile) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  j_java_obj_.Reset(Java_NtpSyncedThemeBridge_create(env, profile));
}

NtpSyncedThemeBridge::NtpSyncedThemeBridge() = default;

NtpSyncedThemeBridge::~NtpSyncedThemeBridge() = default;

void NtpSyncedThemeBridge::OnChromeColorSynced(int color_id,
                                               bool daily_refresh_enabled) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_NtpSyncedThemeBridge_onChromeColorSynced(env, j_java_obj_, color_id,
                                                daily_refresh_enabled);
}

void NtpSyncedThemeBridge::OnDefaultThemeSynced() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_NtpSyncedThemeBridge_onDefaultThemeSynced(env, j_java_obj_);
}

void NtpSyncedThemeBridge::OnThemeSyncReady() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_NtpSyncedThemeBridge_onThemeSyncReady(env, j_java_obj_);
}

void NtpSyncedThemeBridge::OnThemeCollectionUpdated(bool is_sync) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_NtpSyncedThemeBridge_onThemeCollectionUpdated(env, j_java_obj_, is_sync);
}

// Returns the Java NtpSyncedThemeBridge of `profile`, creating its C++ bridge
// on first use.
static ScopedJavaLocalRef<jobject> JNI_NtpSyncedThemeBridge_GetForProfile(
    JNIEnv* env,
    Profile* profile) {
  return GetService(profile)
      ->GetOrCreateSyncedThemeBridge()
      ->java_object()
      .AsLocalRef(env);
}

// Fetches the next image for a theme collection with daily refresh enabled.
static void JNI_NtpSyncedThemeBridge_FetchNextThemeCollectionImage(
    JNIEnv* env,
    Profile* profile) {
  GetService(profile)->RefreshBackgroundIfNeeded();
}

// Fetches the current custom background information (e.g., URL, collection ID)
// from NtpAndroidCustomBackgroundService.
static ScopedJavaLocalRef<jobject>
JNI_NtpSyncedThemeBridge_GetCustomBackgroundInfo(JNIEnv* env,
                                                 Profile* profile) {
  std::optional<CustomBackground> background =
      GetService(profile)->GetCustomBackground();
  if (!background.has_value()) {
    return nullptr;
  }

  return Java_NtpSyncedThemeBridge_createCustomBackgroundInfo(
      env, background->custom_background_url, background->collection_id,
      background->is_uploaded_image, background->daily_refresh_enabled,
      background->custom_background_attribution_line_1,
      background->custom_background_attribution_line_2,
      background->custom_background_attribution_action_url,
      static_cast<int32_t>(
          background->custom_background_main_color.value_or(0)));
}

// Delivers the theme stored in preferences to the bridge of `profile` as if it
// had just arrived from sync.
static void JNI_NtpSyncedThemeBridge_GetStoredContinuousThemeAndNotify(
    JNIEnv* env,
    Profile* profile) {
  GetService(profile)->GetStoredContinuousThemeAndNotify();
}

// Exposes whether the underlying Android theme sync bridge is ready.
static bool JNI_NtpSyncedThemeBridge_IsThemeSyncReady(JNIEnv* env,
                                                      Profile* profile) {
  return GetService(profile)->IsThemeSyncReady();
}

// Sets the Chrome color ID and whether it is refreshed daily on
// NtpAndroidCustomBackgroundService.
static void JNI_NtpSyncedThemeBridge_SetChromeColor(
    JNIEnv* env,
    Profile* profile,
    int color_id,
    bool daily_refresh_enabled) {
  GetService(profile)->SetChromeColor(color_id, daily_refresh_enabled);
}

// Resets the custom background on NtpAndroidCustomBackgroundService.
static void JNI_NtpSyncedThemeBridge_ResetCustomBackgroundInfo(
    JNIEnv* env,
    Profile* profile) {
  GetService(profile)->ResetCustomBackgroundInfo();
}

// Marks custom background as local to device on
// NtpAndroidCustomBackgroundService.
static void JNI_NtpSyncedThemeBridge_SelectLocalBackgroundImage(
    JNIEnv* env,
    Profile* profile) {
  GetService(profile)->SelectLocalBackgroundImage(base::FilePath());
}

// Updates the theme collection background with collection ID, attributions,
// attribution action URL, primary color, and daily refresh state, and notifies
// the sync bridge.
static void JNI_NtpSyncedThemeBridge_UpdateCustomBackgroundPrefsWithColor(
    JNIEnv* env,
    Profile* profile,
    const GURL& url,
    const std::string& collection_id,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& attribution_action_url,
    int32_t primary_color,
    bool is_daily_refresh) {
  GetService(profile)->UpdateThemeCollectionPrefsWithColor(
      url, collection_id, attribution_line_1, attribution_line_2,
      attribution_action_url, static_cast<SkColor>(primary_color),
      is_daily_refresh);
}

DEFINE_JNI(NtpSyncedThemeBridge)
