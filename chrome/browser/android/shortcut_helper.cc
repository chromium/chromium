// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_helper.h"

#include <jni.h>
#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/android/chrome_jni_headers/ShortcutHelper_jni.h"
#include "chrome/browser/android/color_helpers.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/color_analysis.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

int g_ideal_homescreen_icon_size = -1;
int g_minimum_homescreen_icon_size = -1;
int g_ideal_splash_image_size = -1;
int g_minimum_splash_image_size = -1;
int g_ideal_monochrome_icon_size = -1;
int g_ideal_adaptive_launcher_icon_size = -1;
int g_ideal_shortcut_icon_size = -1;

int g_default_rgb_icon_value = 145;

// Retrieves and caches the ideal and minimum sizes of the Home screen icon,
// the splash screen image, and the shortcut icons.
void GetIconSizes() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jintArray> java_size_array =
      Java_ShortcutHelper_getIconSizes(env);
  std::vector<int> sizes;
  base::android::JavaIntArrayToIntVector(env, java_size_array, &sizes);

  // Check that the size returned is what is expected.
  DCHECK_EQ(7u, sizes.size());

  // This ordering must be kept up to date with the Java ShortcutHelper.
  g_ideal_homescreen_icon_size = sizes[0];
  g_minimum_homescreen_icon_size = sizes[1];
  g_ideal_splash_image_size = sizes[2];
  g_minimum_splash_image_size = sizes[3];
  g_ideal_monochrome_icon_size = sizes[4];
  g_ideal_adaptive_launcher_icon_size = sizes[5];
  g_ideal_shortcut_icon_size = sizes[6];

  // Try to ensure that the data returned is sane.
  DCHECK(g_minimum_homescreen_icon_size <= g_ideal_homescreen_icon_size);
  DCHECK(g_minimum_splash_image_size <= g_ideal_splash_image_size);
}

// Adds a shortcut which opens in a fullscreen window to the launcher.
void AddWebappWithSkBitmap(content::WebContents* web_contents,
                           const ShortcutInfo& info,
                           const std::string& webapp_id,
                           const SkBitmap& icon_bitmap,
                           bool is_icon_maskable) {
  // Send the data to the Java side to create the shortcut.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_webapp_id =
      base::android::ConvertUTF8ToJavaString(env, webapp_id);
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_scope_url =
      base::android::ConvertUTF8ToJavaString(env, info.scope.spec());
  ScopedJavaLocalRef<jstring> java_user_title =
      base::android::ConvertUTF16ToJavaString(env, info.user_title);
  ScopedJavaLocalRef<jstring> java_name =
      base::android::ConvertUTF16ToJavaString(env, info.name);
  ScopedJavaLocalRef<jstring> java_short_name =
      base::android::ConvertUTF16ToJavaString(env, info.short_name);
  ScopedJavaLocalRef<jstring> java_best_primary_icon_url =
      base::android::ConvertUTF8ToJavaString(env,
                                             info.best_primary_icon_url.spec());
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (!icon_bitmap.drawsNothing())
    java_bitmap = gfx::ConvertToJavaBitmap(icon_bitmap);

  Java_ShortcutHelper_addWebapp(
      env, java_webapp_id, java_url, java_scope_url, java_user_title, java_name,
      java_short_name, java_best_primary_icon_url, java_bitmap,
      is_icon_maskable, static_cast<int>(info.display),
      static_cast<int>(info.orientation), info.source,
      OptionalSkColorToJavaColor(info.theme_color),
      OptionalSkColorToJavaColor(info.background_color));

  // Start downloading the splash image in parallel with the app install.
  content::ManifestIconDownloader::Download(
      web_contents, info.splash_image_url, info.ideal_splash_image_size_in_px,
      info.minimum_splash_image_size_in_px,
      base::BindOnce(&ShortcutHelper::StoreWebappSplashImage, webapp_id));
}

// Adds a shortcut which opens in a browser tab to the launcher.
void AddShortcutWithSkBitmap(const ShortcutInfo& info,
                             const std::string& id,
                             const SkBitmap& icon_bitmap,
                             bool is_icon_maskable) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_id =
      base::android::ConvertUTF8ToJavaString(env, id);
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_user_title =
      base::android::ConvertUTF16ToJavaString(env, info.user_title);
  ScopedJavaLocalRef<jstring> java_best_primary_icon_url =
      base::android::ConvertUTF8ToJavaString(env,
                                             info.best_primary_icon_url.spec());
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (!icon_bitmap.drawsNothing())
    java_bitmap = gfx::ConvertToJavaBitmap(icon_bitmap);
  Java_ShortcutHelper_addShortcut(env, java_id, java_url, java_user_title,
                                  java_bitmap, is_icon_maskable, info.source,
                                  java_best_primary_icon_url);
}

}  // anonymous namespace

// static
std::unique_ptr<ShortcutInfo> ShortcutHelper::CreateShortcutInfo(
    const GURL& manifest_url,
    const blink::Manifest& manifest,
    const GURL& primary_icon_url) {
  auto shortcut_info = std::make_unique<ShortcutInfo>(GURL());
  if (!manifest.IsEmpty()) {
    shortcut_info->UpdateFromManifest(manifest);
    shortcut_info->manifest_url = manifest_url;
    shortcut_info->best_primary_icon_url = primary_icon_url;
  }

  shortcut_info->ideal_splash_image_size_in_px = GetIdealSplashImageSizeInPx();
  shortcut_info->minimum_splash_image_size_in_px =
      GetMinimumSplashImageSizeInPx();
  shortcut_info->splash_image_url =
      blink::ManifestIconSelector::FindBestMatchingSquareIcon(
          manifest.icons, shortcut_info->ideal_splash_image_size_in_px,
          shortcut_info->minimum_splash_image_size_in_px,
          blink::mojom::ManifestImageResource_Purpose::ANY);

  return shortcut_info;
}

// static
void ShortcutHelper::AddToLauncherWithSkBitmap(
    content::WebContents* web_contents,
    const ShortcutInfo& info,
    const SkBitmap& icon_bitmap,
    bool is_icon_maskable) {
  std::string webapp_id = base::GenerateGUID();
  if (info.display == blink::mojom::DisplayMode::kStandalone ||
      info.display == blink::mojom::DisplayMode::kFullscreen ||
      info.display == blink::mojom::DisplayMode::kMinimalUi) {
    AddWebappWithSkBitmap(web_contents, info, webapp_id, icon_bitmap,
                          is_icon_maskable);
    return;
  }
  AddShortcutWithSkBitmap(info, webapp_id, icon_bitmap, is_icon_maskable);
}

void ShortcutHelper::ShowWebApkInstallInProgressToast() {
  Java_ShortcutHelper_showWebApkInstallInProgressToast(
      base::android::AttachCurrentThread());
}

int ShortcutHelper::GetIdealHomescreenIconSizeInPx() {
  if (g_ideal_homescreen_icon_size == -1)
    GetIconSizes();
  return g_ideal_homescreen_icon_size;
}

int ShortcutHelper::GetMinimumHomescreenIconSizeInPx() {
  if (g_minimum_homescreen_icon_size == -1)
    GetIconSizes();
  return g_minimum_homescreen_icon_size;
}

int ShortcutHelper::GetIdealSplashImageSizeInPx() {
  if (g_ideal_splash_image_size == -1)
    GetIconSizes();
  return g_ideal_splash_image_size;
}

int ShortcutHelper::GetMinimumSplashImageSizeInPx() {
  if (g_minimum_splash_image_size == -1)
    GetIconSizes();
  return g_minimum_splash_image_size;
}

int ShortcutHelper::GetIdealAdaptiveLauncherIconSizeInPx() {
  if (g_ideal_adaptive_launcher_icon_size == -1)
    GetIconSizes();
  return g_ideal_adaptive_launcher_icon_size;
}

int ShortcutHelper::GetIdealShortcutIconSizeInPx() {
  if (g_ideal_shortcut_icon_size == -1)
    GetIconSizes();
  return g_ideal_shortcut_icon_size;
}

// static
void ShortcutHelper::StoreWebappSplashImage(const std::string& webapp_id,
                                            const SkBitmap& splash_image) {
  if (splash_image.drawsNothing())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_webapp_id =
      base::android::ConvertUTF8ToJavaString(env, webapp_id);
  ScopedJavaLocalRef<jobject> java_splash_image =
      gfx::ConvertToJavaBitmap(splash_image);

  Java_ShortcutHelper_storeWebappSplashImage(env, java_webapp_id,
                                             java_splash_image);
}

// static
SkBitmap ShortcutHelper::FinalizeLauncherIconInBackground(
    const SkBitmap& bitmap,
    bool is_icon_maskable,
    const GURL& url,
    bool* is_generated) {
  base::AssertLongCPUWorkAllowed();

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result;
  *is_generated = false;

  if (!bitmap.isNull()) {
    if (Java_ShortcutHelper_isIconLargeEnoughForLauncher(env, bitmap.width(),
                                                         bitmap.height())) {
      ScopedJavaLocalRef<jobject> java_bitmap =
          gfx::ConvertToJavaBitmap(bitmap);
      result = Java_ShortcutHelper_createHomeScreenIconFromWebIcon(
          env, java_bitmap, is_icon_maskable);
    }
  }

  if (result.is_null()) {
    ScopedJavaLocalRef<jstring> java_url =
        base::android::ConvertUTF8ToJavaString(env, url.spec());
    SkColor mean_color =
        SkColorSetRGB(g_default_rgb_icon_value, g_default_rgb_icon_value,
                      g_default_rgb_icon_value);

    if (!bitmap.isNull())
      mean_color = color_utils::CalculateKMeanColorOfBitmap(bitmap);

    *is_generated = true;
    result = Java_ShortcutHelper_generateHomeScreenIcon(
        env, java_url, SkColorGetR(mean_color), SkColorGetG(mean_color),
        SkColorGetB(mean_color));
  }

  return result.obj()
             ? gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(result))
             : SkBitmap();
}

// static
std::string ShortcutHelper::QueryFirstWebApkPackage(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> java_webapk_package_name =
      Java_ShortcutHelper_queryFirstWebApkPackage(env, java_url);

  std::string webapk_package_name;
  if (java_webapk_package_name.obj()) {
    webapk_package_name =
        base::android::ConvertJavaStringToUTF8(env, java_webapk_package_name);
  }
  return webapk_package_name;
}

// static
bool ShortcutHelper::IsWebApkInstalled(content::BrowserContext* browser_context,
                                       const GURL& url) {
  return !QueryFirstWebApkPackage(url).empty();
}

// static
bool ShortcutHelper::DoesOriginContainAnyInstalledWebApk(const GURL& origin) {
  DCHECK_EQ(origin, origin.GetOrigin());
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_origin =
      base::android::ConvertUTF8ToJavaString(env, origin.spec());
  return Java_ShortcutHelper_doesOriginContainAnyInstalledWebApk(env,
                                                                 java_origin);
}

bool ShortcutHelper::DoesOriginContainAnyInstalledTrustedWebActivity(
    const GURL& origin) {
  DCHECK_EQ(origin, origin.GetOrigin());
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_origin =
      base::android::ConvertUTF8ToJavaString(env, origin.spec());
  return Java_ShortcutHelper_doesOriginContainAnyInstalledTwa(env, java_origin);
}

std::set<GURL> ShortcutHelper::GetOriginsWithInstalledWebApksOrTwas() {
  std::set<GURL> installed_origins;
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobjectArray> j_installed_origins =
      Java_ShortcutHelper_getOriginsWithInstalledWebApksOrTwas(env);

  if (j_installed_origins) {
    std::vector<std::string> installed_origins_list;
    base::android::AppendJavaStringArrayToStringVector(env, j_installed_origins,
                                                       &installed_origins_list);
    for (auto& origin : installed_origins_list)
      installed_origins.emplace(GURL(origin));
  }
  return installed_origins;
}

void ShortcutHelper::SetForceWebApkUpdate(const std::string& id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ShortcutHelper_setForceWebApkUpdate(
      env, base::android::ConvertUTF8ToJavaString(env, id));
}

// static
bool ShortcutHelper::DoesAndroidSupportMaskableIcons() {
  return base::FeatureList::IsEnabled(chrome::android::kWebApkAdaptiveIcon) &&
         base::android::BuildInfo::GetInstance()->sdk_int() >=
             base::android::SDK_VERSION_OREO;
}

// static
void ShortcutHelper::SetIdealShortcutSizeForTesting(int size) {
  g_ideal_shortcut_icon_size = size;
}
