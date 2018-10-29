// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_helper.h"

#include <jni.h>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/android/color_helpers.h"
#include "chrome/browser/android/webapk/chrome_webapk_host.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "jni/ShortcutHelper_jni.h"
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
int g_ideal_badge_icon_size = -1;

int g_default_rgb_icon_value = 145;

// Retrieves and caches the ideal and minimum sizes of the Home screen icon
// and the splash screen image.
void GetHomescreenIconAndSplashImageSizes() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jintArray> java_size_array =
      Java_ShortcutHelper_getHomeScreenIconAndSplashImageSizes(env);
  std::vector<int> sizes;
  base::android::JavaIntArrayToIntVector(env, java_size_array, &sizes);

  // Check that the size returned is what is expected.
  DCHECK(sizes.size() == 5);

  // This ordering must be kept up to date with the Java ShortcutHelper.
  g_ideal_homescreen_icon_size = sizes[0];
  g_minimum_homescreen_icon_size = sizes[1];
  g_ideal_splash_image_size = sizes[2];
  g_minimum_splash_image_size = sizes[3];
  g_ideal_badge_icon_size = sizes[4];

  // Try to ensure that the data returned is sane.
  DCHECK(g_minimum_homescreen_icon_size <= g_ideal_homescreen_icon_size);
  DCHECK(g_minimum_splash_image_size <= g_ideal_splash_image_size);
}

// Adds a shortcut which opens in a fullscreen window to the launcher.
// |splash_image_callback| will be invoked once the Java-side operation has
// completed. This is necessary as Java will asynchronously create and
// populate a WebappDataStorage object for standalone-capable sites. This must
// exist before the splash image can be stored.
void AddWebappWithSkBitmap(const ShortcutInfo& info,
                           const std::string& webapp_id,
                           const SkBitmap& icon_bitmap,
                           const base::Closure& splash_image_callback) {
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
  ScopedJavaLocalRef<jstring> java_splash_screen_url =
      base::android::ConvertUTF8ToJavaString(env,
                                             info.splash_screen_url.spec());
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (!icon_bitmap.drawsNothing())
    java_bitmap = gfx::ConvertToJavaBitmap(&icon_bitmap);

  // The callback will need to be run after shortcut creation completes in order
  // to download the splash image and save it to the WebappDataStorage. Create a
  // copy of the callback here and send the pointer to Java, which will send it
  // back once the asynchronous shortcut creation process finishes.
  uintptr_t callback_pointer =
      reinterpret_cast<uintptr_t>(new base::Closure(splash_image_callback));

  Java_ShortcutHelper_addWebapp(
      env, java_webapp_id, java_url, java_scope_url, java_user_title, java_name,
      java_short_name, java_best_primary_icon_url, java_bitmap, info.display,
      info.orientation, info.source,
      OptionalSkColorToJavaColor(info.theme_color),
      OptionalSkColorToJavaColor(info.background_color), java_splash_screen_url,
      callback_pointer);
}

// Adds a shortcut which opens in a browser tab to the launcher.
void AddShortcutWithSkBitmap(const ShortcutInfo& info,
                             const std::string& id,
                             const SkBitmap& icon_bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_id =
      base::android::ConvertUTF8ToJavaString(env, id);
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info.url.spec());
  ScopedJavaLocalRef<jstring> java_user_title =
      base::android::ConvertUTF16ToJavaString(env, info.user_title);
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (!icon_bitmap.drawsNothing())
    java_bitmap = gfx::ConvertToJavaBitmap(&icon_bitmap);

  Java_ShortcutHelper_addShortcut(env, java_id, java_url, java_user_title,
                                  java_bitmap, info.source);
}

}  // anonymous namespace

// static
std::unique_ptr<ShortcutInfo> ShortcutHelper::CreateShortcutInfo(
    const GURL& manifest_url,
    const blink::Manifest& manifest,
    const GURL& primary_icon_url,
    const GURL& badge_icon_url) {
  auto shortcut_info = std::make_unique<ShortcutInfo>(GURL());
  if (!manifest.IsEmpty()) {
    shortcut_info->UpdateFromManifest(manifest);
    shortcut_info->manifest_url = manifest_url;
    shortcut_info->best_primary_icon_url = primary_icon_url;
    shortcut_info->best_badge_icon_url = badge_icon_url;
  }

  shortcut_info->ideal_splash_image_size_in_px = GetIdealSplashImageSizeInPx();
  shortcut_info->minimum_splash_image_size_in_px =
      GetMinimumSplashImageSizeInPx();
  shortcut_info->splash_image_url =
      blink::ManifestIconSelector::FindBestMatchingIcon(
          manifest.icons, shortcut_info->ideal_splash_image_size_in_px,
          shortcut_info->minimum_splash_image_size_in_px,
          blink::Manifest::ImageResource::Purpose::ANY);

  return shortcut_info;
}

// static
void ShortcutHelper::AddToLauncherWithSkBitmap(
    content::WebContents* web_contents,
    const ShortcutInfo& info,
    const SkBitmap& icon_bitmap) {
  std::string webapp_id = base::GenerateGUID();
  if (info.display == blink::kWebDisplayModeStandalone ||
      info.display == blink::kWebDisplayModeFullscreen ||
      info.display == blink::kWebDisplayModeMinimalUi) {
    AddWebappWithSkBitmap(
        info, webapp_id, icon_bitmap,
        base::Bind(&ShortcutHelper::FetchSplashScreenImage, web_contents,
                   info.splash_image_url, info.ideal_splash_image_size_in_px,
                   info.minimum_splash_image_size_in_px, webapp_id));
    return;
  }
  AddShortcutWithSkBitmap(info, webapp_id, icon_bitmap);
}

void ShortcutHelper::ShowWebApkInstallInProgressToast() {
  Java_ShortcutHelper_showWebApkInstallInProgressToast(
      base::android::AttachCurrentThread());
}

int ShortcutHelper::GetIdealHomescreenIconSizeInPx() {
  if (g_ideal_homescreen_icon_size == -1)
    GetHomescreenIconAndSplashImageSizes();
  return g_ideal_homescreen_icon_size;
}

int ShortcutHelper::GetMinimumHomescreenIconSizeInPx() {
  if (g_minimum_homescreen_icon_size == -1)
    GetHomescreenIconAndSplashImageSizes();
  return g_minimum_homescreen_icon_size;
}

int ShortcutHelper::GetIdealSplashImageSizeInPx() {
  if (g_ideal_splash_image_size == -1)
    GetHomescreenIconAndSplashImageSizes();
  return g_ideal_splash_image_size;
}

int ShortcutHelper::GetMinimumSplashImageSizeInPx() {
  if (g_minimum_splash_image_size == -1)
    GetHomescreenIconAndSplashImageSizes();
  return g_minimum_splash_image_size;
}

int ShortcutHelper::GetIdealBadgeIconSizeInPx() {
  if (g_ideal_badge_icon_size == -1)
    GetHomescreenIconAndSplashImageSizes();
  return g_ideal_badge_icon_size;
}

// static
void ShortcutHelper::FetchSplashScreenImage(
    content::WebContents* web_contents,
    const GURL& image_url,
    const int ideal_splash_image_size_in_px,
    const int minimum_splash_image_size_in_px,
    const std::string& webapp_id) {
  // This is a fire and forget task. It is not vital for the splash screen image
  // to be downloaded so if the downloader returns false there is no fallback.
  content::ManifestIconDownloader::Download(
      web_contents, image_url, ideal_splash_image_size_in_px,
      minimum_splash_image_size_in_px,
      base::Bind(&ShortcutHelper::StoreWebappSplashImage, webapp_id));
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
      gfx::ConvertToJavaBitmap(&splash_image);

  Java_ShortcutHelper_storeWebappSplashImage(env, java_webapp_id,
                                             java_splash_image);
}

// static
SkBitmap ShortcutHelper::FinalizeLauncherIconInBackground(
    const SkBitmap& bitmap,
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
          gfx::ConvertToJavaBitmap(&bitmap);
      result =
          Java_ShortcutHelper_createHomeScreenIconFromWebIcon(env, java_bitmap);
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
std::string ShortcutHelper::QueryWebApkPackage(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> java_webapk_package_name =
      Java_ShortcutHelper_queryWebApkPackage(env, java_url);

  std::string webapk_package_name = "";
  if (java_webapk_package_name.obj()) {
    webapk_package_name =
        base::android::ConvertJavaStringToUTF8(env, java_webapk_package_name);
  }
  return webapk_package_name;
}

// static
bool ShortcutHelper::IsWebApkInstalled(content::BrowserContext* browser_context,
                                       const GURL& start_url,
                                       const GURL& manifest_url) {
  return !QueryWebApkPackage(start_url).empty() ||
         WebApkInstallService::Get(browser_context)
             ->IsInstallInProgress(manifest_url);
}

GURL ShortcutHelper::GetScopeFromURL(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> java_scope_url =
      Java_ShortcutHelper_getScopeFromUrl(env, java_url);
  return GURL(base::android::ConvertJavaStringToUTF16(env, java_scope_url));
}

void ShortcutHelper::RetrieveWebApks(const WebApkInfoCallback& callback) {
  uintptr_t callback_pointer =
      reinterpret_cast<uintptr_t>(new WebApkInfoCallback(callback));
  Java_ShortcutHelper_retrieveWebApks(base::android::AttachCurrentThread(),
                                      callback_pointer);
}

// Callback used by Java when the shortcut has been created.
// |splash_image_callback| is a pointer to a base::Closure allocated in
// AddShortcutWithSkBitmap, so reinterpret_cast it back and run it.
//
// This callback should only ever be called when the shortcut was for a
// webapp-capable site; otherwise, |splash_image_callback| will have never been
// allocated and doesn't need to be run or deleted.
void JNI_ShortcutHelper_OnWebappDataStored(JNIEnv* env,
                                           const JavaParamRef<jclass>& clazz,
                                           jlong jsplash_image_callback) {
  DCHECK(jsplash_image_callback);
  base::Closure* splash_image_callback =
      reinterpret_cast<base::Closure*>(jsplash_image_callback);
  splash_image_callback->Run();
  delete splash_image_callback;
}

void JNI_ShortcutHelper_OnWebApksRetrieved(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const jlong jcallback_pointer,
    const JavaParamRef<jobjectArray>& jnames,
    const JavaParamRef<jobjectArray>& jshort_names,
    const JavaParamRef<jobjectArray>& jpackage_names,
    const JavaParamRef<jintArray>& jshell_apk_versions,
    const JavaParamRef<jintArray>& jversion_codes,
    const JavaParamRef<jobjectArray>& juris,
    const JavaParamRef<jobjectArray>& jscopes,
    const JavaParamRef<jobjectArray>& jmanifest_urls,
    const JavaParamRef<jobjectArray>& jmanifest_start_urls,
    const JavaParamRef<jintArray>& jdisplay_modes,
    const JavaParamRef<jintArray>& jorientations,
    const JavaParamRef<jlongArray>& jtheme_colors,
    const JavaParamRef<jlongArray>& jbackground_colors,
    const JavaParamRef<jlongArray>& jlast_update_check_times_ms,
    const JavaParamRef<jbooleanArray>& jrelax_updates) {
  DCHECK(jcallback_pointer);
  std::vector<std::string> names;
  base::android::AppendJavaStringArrayToStringVector(env, jnames, &names);
  std::vector<std::string> short_names;
  base::android::AppendJavaStringArrayToStringVector(env, jshort_names,
                                                     &short_names);
  std::vector<std::string> package_names;
  base::android::AppendJavaStringArrayToStringVector(env, jpackage_names,
                                                     &package_names);
  std::vector<int> shell_apk_versions;
  base::android::JavaIntArrayToIntVector(env, jshell_apk_versions,
                                         &shell_apk_versions);
  std::vector<int> version_codes;
  base::android::JavaIntArrayToIntVector(env, jversion_codes, &version_codes);
  std::vector<std::string> uris;
  base::android::AppendJavaStringArrayToStringVector(env, juris, &uris);
  std::vector<std::string> scopes;
  base::android::AppendJavaStringArrayToStringVector(env, jscopes, &scopes);
  std::vector<std::string> manifest_urls;
  base::android::AppendJavaStringArrayToStringVector(env, jmanifest_urls,
                                                     &manifest_urls);
  std::vector<std::string> manifest_start_urls;
  base::android::AppendJavaStringArrayToStringVector(env, jmanifest_start_urls,
                                                     &manifest_start_urls);
  std::vector<int> display_modes;
  base::android::JavaIntArrayToIntVector(env, jdisplay_modes, &display_modes);
  std::vector<int> orientations;
  base::android::JavaIntArrayToIntVector(env, jorientations, &orientations);
  std::vector<int64_t> theme_colors;
  base::android::JavaLongArrayToInt64Vector(env, jtheme_colors, &theme_colors);
  std::vector<int64_t> background_colors;
  base::android::JavaLongArrayToInt64Vector(env, jbackground_colors,
                                            &background_colors);
  std::vector<int64_t> last_update_check_times_ms;
  base::android::JavaLongArrayToInt64Vector(env, jlast_update_check_times_ms,
                                            &last_update_check_times_ms);
  std::vector<bool> relax_updates;
  base::android::JavaBooleanArrayToBoolVector(env, jrelax_updates,
                                              &relax_updates);

  DCHECK(short_names.size() == names.size());
  DCHECK(short_names.size() == package_names.size());
  DCHECK(short_names.size() == shell_apk_versions.size());
  DCHECK(short_names.size() == version_codes.size());
  DCHECK(short_names.size() == uris.size());
  DCHECK(short_names.size() == scopes.size());
  DCHECK(short_names.size() == manifest_urls.size());
  DCHECK(short_names.size() == manifest_start_urls.size());
  DCHECK(short_names.size() == display_modes.size());
  DCHECK(short_names.size() == orientations.size());
  DCHECK(short_names.size() == theme_colors.size());
  DCHECK(short_names.size() == background_colors.size());
  DCHECK(short_names.size() == last_update_check_times_ms.size());
  DCHECK(short_names.size() == relax_updates.size());

  std::vector<WebApkInfo> webapk_list;
  webapk_list.reserve(short_names.size());
  for (size_t i = 0; i < short_names.size(); ++i) {
    webapk_list.push_back(WebApkInfo(
        std::move(names[i]), std::move(short_names[i]),
        std::move(package_names[i]), shell_apk_versions[i], version_codes[i],
        std::move(uris[i]), std::move(scopes[i]), std::move(manifest_urls[i]),
        std::move(manifest_start_urls[i]),
        static_cast<blink::WebDisplayMode>(display_modes[i]),
        static_cast<blink::WebScreenOrientationLockType>(orientations[i]),
        JavaColorToOptionalSkColor(theme_colors[i]),
        JavaColorToOptionalSkColor(background_colors[i]),
        base::Time::FromJavaTime(last_update_check_times_ms[i]),
        relax_updates[i]));
  }

  ShortcutHelper::WebApkInfoCallback* webapk_list_callback =
      reinterpret_cast<ShortcutHelper::WebApkInfoCallback*>(jcallback_pointer);
  webapk_list_callback->Run(webapk_list);
  delete webapk_list_callback;
}
