// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_helper.h"

#include <jni.h>
#include <limits>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/uuid.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/android/color_utils_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ShortcutHelper_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// Adds a shortcut which opens in a fullscreen window to the launcher.
void AddWebappWithSkBitmap(content::WebContents* web_contents,
                           const webapps::ShortcutInfo& info,
                           const std::string& webapp_id,
                           const SkBitmap& icon_bitmap) {
  // Send the data to the Java side to create the shortcut.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (!icon_bitmap.drawsNothing())
    java_bitmap = gfx::ConvertToJavaBitmap(icon_bitmap);

  Java_ShortcutHelper_addWebapp(
      env, webapp_id, info.url.spec(), info.scope.spec(), info.user_title,
      info.name, info.short_name, info.best_primary_icon_url.spec(),
      java_bitmap, info.is_primary_icon_maskable,
      static_cast<int>(info.display), static_cast<int>(info.orientation),
      ui::OptionalSkColorToJavaColor(info.theme_color),
      ui::OptionalSkColorToJavaColor(info.background_color));

  // Start downloading the splash image in parallel with the app install.
  content::ManifestIconDownloader::Download(
      web_contents, info.splash_image_url, info.ideal_splash_image_size_in_px,
      info.minimum_splash_image_size_in_px,
      /* maximum_icon_size_in_px= */ std::numeric_limits<int>::max(),
      base::BindOnce(&ShortcutHelper::StoreWebappSplashImage, webapp_id));
}

// Adds a shortcut which opens in a browser tab to the launcher.
void AddShortcutWithSkBitmap(const webapps::ShortcutInfo& info,
                             const std::string& id,
                             const SkBitmap& icon_bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (!icon_bitmap.drawsNothing())
    java_bitmap = gfx::ConvertToJavaBitmap(icon_bitmap);
  Java_ShortcutHelper_addShortcut(env, id, info.url.spec(), info.user_title,
                                  java_bitmap, info.is_primary_icon_maskable,
                                  info.best_primary_icon_url.spec());
}

void RecordAddToHomeScreenUKM(
    content::WebContents* web_contents,
    const webapps::ShortcutInfo& info,
    webapps::InstallableStatusCode installable_status) {
  if (!web_contents)
    return;

  ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::Webapp_AddToHomeScreen(source_id)
      .SetDisplayMode(static_cast<int>(info.display))
      .SetShortcutReason(static_cast<int>(installable_status))
      .SetSameAsManifestStartUrl(info.url.spec() ==
                                 web_contents->GetLastCommittedURL().spec())
      .Record(ukm::UkmRecorder::Get());
}

}  // anonymous namespace

// static
void ShortcutHelper::AddToLauncherWithSkBitmap(
    content::WebContents* web_contents,
    const webapps::ShortcutInfo& info,
    const SkBitmap& icon_bitmap,
    webapps::InstallableStatusCode installable_status) {
  RecordAddToHomeScreenUKM(web_contents, info, installable_status);

  std::string webapp_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  if (info.display == blink::mojom::DisplayMode::kStandalone ||
      info.display == blink::mojom::DisplayMode::kFullscreen ||
      info.display == blink::mojom::DisplayMode::kMinimalUi) {
    AddWebappWithSkBitmap(web_contents, info, webapp_id, icon_bitmap);
    return;
  }
  AddShortcutWithSkBitmap(info, webapp_id, icon_bitmap);
}

// static
void ShortcutHelper::StoreWebappSplashImage(const std::string& webapp_id,
                                            const SkBitmap& splash_image) {
  if (splash_image.drawsNothing())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ShortcutHelper_storeWebappSplashImage(
      env, webapp_id, gfx::ConvertToJavaBitmap(splash_image));
}

// static
bool ShortcutHelper::DoesOriginContainAnyInstalledWebApk(const GURL& origin) {
  DCHECK_EQ(origin, origin.DeprecatedGetOriginAsURL());
  return Java_ShortcutHelper_doesOriginContainAnyInstalledWebApk(
      base::android::AttachCurrentThread(),
      url::Origin::Create(origin).Serialize());
}

bool ShortcutHelper::DoesOriginContainAnyInstalledTrustedWebActivity(
    const GURL& origin) {
  DCHECK_EQ(origin, origin.DeprecatedGetOriginAsURL());
  return Java_ShortcutHelper_doesOriginContainAnyInstalledTwa(
      base::android::AttachCurrentThread(),
      url::Origin::Create(origin).Serialize());
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
  Java_ShortcutHelper_setForceWebApkUpdate(base::android::AttachCurrentThread(),
                                           id);
}
