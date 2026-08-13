// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/cross_device_theme_tracker_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/ntp_customization/jni_headers/CrossDeviceThemeTracker_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/cross_device_theme_tracker_factory.h"
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/jni_zero/system_jni/List_jni.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/dynamic_color/palette.h"
#include "ui/color/dynamic_color/palette_factory.h"

using base::android::AttachCurrentThread;

namespace themes {

CrossDeviceThemeTrackerAndroid::CrossDeviceThemeTrackerAndroid(
    syncer::DeviceInfoTracker* device_info_tracker)
    : CrossDeviceThemeTracker<sync_pb::ThemeAndroidSpecifics>(
          device_info_tracker) {
  JNIEnv* env = AttachCurrentThread();
  // C++ creates and owns the Java counterpart.
  java_object_.Reset(CrossDeviceThemeTrackerJni::create(
      env, reinterpret_cast<intptr_t>(this)));

  AddObserver(this);
}

CrossDeviceThemeTrackerAndroid::~CrossDeviceThemeTrackerAndroid() {
  JNIEnv* env = AttachCurrentThread();
  // Clear the native pointer on Java side to prevent calls into
  // destroyed C++ object.
  java_object_->clearNativePtr(env);
}

void CrossDeviceThemeTrackerAndroid::OnCrossDeviceThemeChanged() {
  cached_java_themes_.Reset();
  JNIEnv* env = AttachCurrentThread();
  java_object_->notifyThemesChanged(env);
}

void CrossDeviceThemeTrackerAndroid::OnServiceStatusChanged(
    ServiceStatus status) {
  JNIEnv* env = AttachCurrentThread();
  java_object_->notifyStatusChanged(env, status);
}

jni_zero::ScopedJavaLocalRef<JList<JNtpBackgroundDataBase>>
CrossDeviceThemeTrackerAndroid::GetThemes(
    JNIEnv* env,
    const jni_zero::JavaRef<JContext>& jcontext) {
  if (!cached_java_themes_) {
    RecreateJavaThemes(env, jcontext);
  }
  return cached_java_themes_.AsLocalRef(env);
}

ServiceStatus CrossDeviceThemeTrackerAndroid::GetServiceStatus(JNIEnv* env) {
  return CrossDeviceThemeTracker<
      sync_pb::ThemeAndroidSpecifics>::GetServiceStatus();
}

jni_zero::ScopedJavaLocalRef<jobject>
CrossDeviceThemeTrackerAndroid::CreateJavaTheme(
    JNIEnv* env,
    const jni_zero::JavaRef<JContext>& jcontext,
    const DeviceThemeInfo<sync_pb::ThemeAndroidSpecifics>& theme_info) {
  int32_t platform_type = static_cast<int32_t>(
      MapToPlatformType(theme_info.os_type, theme_info.form_factor));

  const sync_pb::ThemeAndroidSpecifics& specifics = theme_info.theme;

  // 1. Extract background data if present.
  bool has_background = specifics.has_ntp_background();
  std::string bg_url;
  std::string bg_collection_id;
  bool is_bg_daily_refresh = false;
  if (has_background) {
    const sync_pb::NtpCustomBackground& bg = specifics.ntp_background();
    bg_url = bg.url();
    bg_collection_id = bg.collection_id();
    is_bg_daily_refresh = bg.has_refresh_timestamp_unix_epoch_seconds();
  }

  // 2. Extract color data if present.
  bool has_chrome_color = specifics.has_chrome_color_info();
  int32_t chrome_color_id = 0;
  bool is_color_daily_refresh = false;

  bool has_user_color = false;
  SkColor primary_light = 0;
  SkColor primary_dark = 0;
  SkColor background_light = 0;
  SkColor background_dark = 0;

  if (has_chrome_color) {
    const sync_pb::ChromeColorInfo& color_info = specifics.chrome_color_info();
    chrome_color_id = color_info.theme_color_id();
    is_color_daily_refresh =
        color_info.has_last_daily_update_timestamp_unix_epoch_millis();
  } else if (specifics.has_user_color_theme() &&
             specifics.user_color_theme().has_color()) {
    has_user_color = true;
    // TODO(crbug.com/517615321): Refactor this palette generation logic to a
    // shared helper function in chrome/browser/themes/theme_service_utils.h
    // so it can be shared with other theme consumers.
    const sync_pb::UserColorTheme& user_color = specifics.user_color_theme();
    SkColor seed_color = static_cast<SkColor>(user_color.color());

    ui::ColorProviderKey::SchemeVariant scheme_variant =
        ui::ColorProviderKey::SchemeVariant::kTonalSpot;
    bool is_baseline = false;
    if (user_color.has_browser_color_variant()) {
      switch (user_color.browser_color_variant()) {
        case sync_pb::UserColorTheme_BrowserColorVariant_SYSTEM:
        case sync_pb::
            UserColorTheme_BrowserColorVariant_BROWSER_COLOR_VARIANT_UNSPECIFIED:
          is_baseline = true;
          break;
        case sync_pb::UserColorTheme_BrowserColorVariant_NEUTRAL:
          scheme_variant = ui::ColorProviderKey::SchemeVariant::kNeutral;
          break;
        case sync_pb::UserColorTheme_BrowserColorVariant_VIBRANT:
          scheme_variant = ui::ColorProviderKey::SchemeVariant::kVibrant;
          break;
        case sync_pb::UserColorTheme_BrowserColorVariant_EXPRESSIVE:
          scheme_variant = ui::ColorProviderKey::SchemeVariant::kExpressive;
          break;
        case sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT:
        default:
          scheme_variant = ui::ColorProviderKey::SchemeVariant::kTonalSpot;
          break;
      }
    } else {
      is_baseline = true;
    }

    std::unique_ptr<ui::Palette> palette =
        ui::GeneratePalette(seed_color, scheme_variant);
    primary_light = palette->primary().get(40);
    primary_dark = palette->primary().get(80);
    background_light =
        is_baseline ? palette->neutral().get(100) : palette->neutral().get(98);
    background_dark =
        is_baseline ? palette->neutral().get(25) : palette->secondary().get(25);
  }

  // 3. Send extracted info up to Java.
  if (has_background) {
    return CrossDeviceThemeTrackerJni::createThemeCollectionData(
        env, jcontext, platform_type, bg_url, bg_collection_id,
        is_bg_daily_refresh, has_chrome_color, chrome_color_id, has_user_color,
        static_cast<int32_t>(primary_light));
  } else if (has_chrome_color) {
    return CrossDeviceThemeTrackerJni::createColorData(
        env, jcontext, platform_type, chrome_color_id, is_color_daily_refresh);
  } else if (has_user_color) {
    return CrossDeviceThemeTrackerJni::createCustomizedColorData(
        env, jcontext, platform_type, static_cast<int32_t>(primary_light),
        static_cast<int32_t>(primary_dark),
        static_cast<int32_t>(background_light),
        static_cast<int32_t>(background_dark));
  }
  return nullptr;
}

jni_zero::ScopedJavaLocalRef<jobject>
CrossDeviceThemeTrackerAndroid::GetThemeForDeviceGuid(
    JNIEnv* env,
    const jni_zero::JavaRef<JContext>& jcontext,
    const std::string& device_guid) {
  auto other_themes = GetOtherDevicesThemes();
  if (other_themes.empty()) {
    return nullptr;
  }

  // 1. If a specific device_guid is provided, only return the theme matching
  // that GUID. If that device does not have a theme (e.g. Themes sync was off
  // on that device or default theme was used), return nullptr rather than
  // falling back to another device's theme, to avoid combining preferences from
  // one device and a theme from another.
  if (!device_guid.empty()) {
    for (const auto& theme_info : other_themes) {
      if (theme_info.guid == device_guid) {
        return CreateJavaTheme(env, jcontext, theme_info);
      }
    }
    return nullptr;
  }

  // 2. If no device_guid was specified ("") (i.e. no preferences are being
  // imported from any device), select the best candidate theme across all
  // available devices. Prefer Android (same platform) first: when Themes sync
  // is enabled, Android devices continuously auto-sync their themes, so
  // aligning with the existing Android theme allows the importer to recognize
  // that local and remote themes are already identical, avoiding redundant
  // snackbar prompts and minimizing churn in synced theme colors.
  const auto* best_theme = &other_themes.front();
  for (const auto& theme_info : other_themes) {
    if (theme_info.os_type == syncer::DeviceInfo::OsType::kAndroid) {
      best_theme = &theme_info;
      break;
    }
  }
  return CreateJavaTheme(env, jcontext, *best_theme);
}

void CrossDeviceThemeTrackerAndroid::RecreateJavaThemes(
    JNIEnv* env,
    const jni_zero::JavaRef<JContext>& jcontext) {
  auto other_themes = GetOtherDevicesThemes();

  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> java_themes_vector;

  for (const auto& theme_info : other_themes) {
    auto java_theme = CreateJavaTheme(env, jcontext, theme_info);
    if (java_theme) {
      java_themes_vector.emplace_back(std::move(java_theme));
    }
  }

  auto java_themes = jni_zero::ToJniList(env, java_themes_vector)
                         .As<JList<JNtpBackgroundDataBase>>();

  cached_java_themes_.Reset(java_themes);
}

PlatformType CrossDeviceThemeTrackerAndroid::MapToPlatformType(
    syncer::DeviceInfo::OsType os_type,
    syncer::DeviceInfo::FormFactor form_factor) {
  switch (os_type) {
    case syncer::DeviceInfo::OsType::kAndroid:
      if (form_factor == syncer::DeviceInfo::FormFactor::kDesktop) {
        return PlatformType::kDesktop;
      }
      return PlatformType::kAndroid;
    case syncer::DeviceInfo::OsType::kIOS:
      return PlatformType::kIos;
    case syncer::DeviceInfo::OsType::kWindows:
    case syncer::DeviceInfo::OsType::kMac:
    case syncer::DeviceInfo::OsType::kLinux:
    case syncer::DeviceInfo::OsType::kChromeOsAsh:
    case syncer::DeviceInfo::OsType::kChromeOsLacros:
      return PlatformType::kDesktop;
    default:
      return PlatformType::kUnknown;
  }
}

}  // namespace themes

static jni_zero::ScopedJavaLocalRef<JCrossDeviceThemeTracker>
JNI_CrossDeviceThemeTracker_GetForProfile(JNIEnv* env, Profile* profile) {
  auto* tracker = CrossDeviceThemeTrackerFactory::GetForProfile(profile);
  if (!tracker) {
    return nullptr;
  }
  return static_cast<themes::CrossDeviceThemeTrackerAndroid*>(tracker)
      ->java_object()
      .AsLocalRef(env);
}

DEFINE_JNI(CrossDeviceThemeTracker)
