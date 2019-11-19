// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <memory>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/android/chrome_jni_headers/WebApkUpdateManager_jni.h"
#include "chrome/browser/android/color_helpers.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

namespace {

// Called after the update either succeeds or fails.
void OnUpdated(const JavaRef<jobject>& java_callback,
               WebApkInstallResult result,
               bool relax_updates,
               const std::string& webapk_package) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebApkUpdateCallback_onResultFromNative(
      env, java_callback, static_cast<int>(result), relax_updates);
}

}  // anonymous namespace

// static JNI method.
static void JNI_WebApkUpdateManager_StoreWebApkUpdateRequestToFile(
    JNIEnv* env,
    const JavaParamRef<jstring>& java_update_request_path,
    const JavaParamRef<jstring>& java_start_url,
    const JavaParamRef<jstring>& java_scope,
    const JavaParamRef<jstring>& java_name,
    const JavaParamRef<jstring>& java_short_name,
    const JavaParamRef<jstring>& java_primary_icon_url,
    const JavaParamRef<jobject>& java_primary_icon_bitmap,
    jboolean java_is_primary_icon_maskable,
    const JavaParamRef<jstring>& java_badge_icon_url,
    const JavaParamRef<jobject>& java_badge_icon_bitmap,
    const JavaParamRef<jobjectArray>& java_icon_urls,
    const JavaParamRef<jobjectArray>& java_icon_hashes,
    jint java_display_mode,
    jint java_orientation,
    jlong java_theme_color,
    jlong java_background_color,
    const JavaParamRef<jstring>& java_share_target_action,
    const JavaParamRef<jstring>& java_share_target_param_title,
    const JavaParamRef<jstring>& java_share_target_param_text,
    const jboolean java_share_target_param_is_method_post,
    const jboolean java_share_target_param_is_enctype_multipart,
    const JavaParamRef<jobjectArray>& java_share_target_param_file_names,
    const JavaParamRef<jobjectArray>& java_share_target_param_accepts,
    const JavaParamRef<jstring>& java_web_manifest_url,
    const JavaParamRef<jstring>& java_webapk_package,
    jint java_webapk_version,
    jboolean java_is_manifest_stale,
    jint java_update_reason,
    const JavaParamRef<jobject>& java_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string update_request_path =
      ConvertJavaStringToUTF8(env, java_update_request_path);

  ShortcutInfo info(GURL(ConvertJavaStringToUTF8(env, java_start_url)));
  info.scope = GURL(ConvertJavaStringToUTF8(env, java_scope));
  info.name = ConvertJavaStringToUTF16(env, java_name);
  info.short_name = ConvertJavaStringToUTF16(env, java_short_name);
  info.user_title = info.short_name;
  info.display = static_cast<blink::mojom::DisplayMode>(java_display_mode);
  info.orientation =
      static_cast<blink::WebScreenOrientationLockType>(java_orientation);
  info.theme_color = JavaColorToOptionalSkColor(java_theme_color);
  info.background_color = JavaColorToOptionalSkColor(java_background_color);
  info.best_primary_icon_url =
      GURL(ConvertJavaStringToUTF8(env, java_primary_icon_url));
  info.best_badge_icon_url =
      GURL(ConvertJavaStringToUTF8(env, java_badge_icon_url));
  info.manifest_url = GURL(ConvertJavaStringToUTF8(env, java_web_manifest_url));

  GURL share_target_action =
      GURL(ConvertJavaStringToUTF8(env, java_share_target_action));
  if (!share_target_action.is_empty()) {
    info.share_target = ShareTarget();
    info.share_target->action = share_target_action;
    info.share_target->params.title =
        ConvertJavaStringToUTF16(java_share_target_param_title);
    info.share_target->params.text =
        ConvertJavaStringToUTF16(java_share_target_param_text);
    info.share_target->method =
        java_share_target_param_is_method_post == JNI_TRUE
            ? blink::Manifest::ShareTarget::Method::kPost
            : blink::Manifest::ShareTarget::Method::kGet;

    info.share_target->enctype =
        java_share_target_param_is_enctype_multipart == JNI_TRUE
            ? blink::Manifest::ShareTarget::Enctype::kMultipartFormData
            : blink::Manifest::ShareTarget::Enctype::kFormUrlEncoded;

    std::vector<base::string16> fileNames;
    base::android::AppendJavaStringArrayToStringVector(
        env, java_share_target_param_file_names, &fileNames);

    std::vector<std::vector<base::string16>> accepts;
    base::android::Java2dStringArrayTo2dStringVector(
        env, java_share_target_param_accepts, &accepts);

    // The length of fileNames and accepts should always be the same, but here
    // we just want to be safe.
    for (size_t i = 0; i < std::min(fileNames.size(), accepts.size()); ++i) {
      ShareTargetParamsFile file;
      file.name = fileNames[i];
      file.accept.swap(accepts[i]);
      info.share_target->params.files.push_back(file);
    }
  }

  base::android::AppendJavaStringArrayToStringVector(env, java_icon_urls,
                                                     &info.icon_urls);

  std::vector<std::string> icon_hashes;
  base::android::AppendJavaStringArrayToStringVector(env, java_icon_hashes,
                                                     &icon_hashes);

  std::map<std::string, std::string> icon_url_to_murmur2_hash;
  for (size_t i = 0; i < info.icon_urls.size(); ++i)
    icon_url_to_murmur2_hash[info.icon_urls[i]] = icon_hashes[i];

  gfx::JavaBitmap java_primary_icon_bitmap_lock(java_primary_icon_bitmap);
  SkBitmap primary_icon =
      gfx::CreateSkBitmapFromJavaBitmap(java_primary_icon_bitmap_lock);
  primary_icon.setImmutable();

  SkBitmap badge_icon;
  if (!java_badge_icon_bitmap.is_null()) {
    gfx::JavaBitmap java_badge_icon_bitmap_lock(java_badge_icon_bitmap);
    gfx::CreateSkBitmapFromJavaBitmap(java_badge_icon_bitmap_lock);
    badge_icon.setImmutable();
  }

  std::string webapk_package;
  ConvertJavaStringToUTF8(env, java_webapk_package, &webapk_package);

  WebApkUpdateReason update_reason =
      static_cast<WebApkUpdateReason>(java_update_reason);

  WebApkInstaller::StoreUpdateRequestToFile(
      base::FilePath(update_request_path), info, primary_icon,
      java_is_primary_icon_maskable, badge_icon, webapk_package,
      std::to_string(java_webapk_version), icon_url_to_murmur2_hash,
      java_is_manifest_stale, update_reason,
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(java_callback)));
}

// static JNI method.
static void JNI_WebApkUpdateManager_UpdateWebApkFromFile(
    JNIEnv* env,
    const JavaParamRef<jstring>& java_update_request_path,
    const JavaParamRef<jobject>& java_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScopedJavaGlobalRef<jobject> callback_ref(java_callback);

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == nullptr) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&OnUpdated, callback_ref, WebApkInstallResult::FAILURE,
                       false /* relax_updates */, "" /* webapk_package */));
    return;
  }

  std::string update_request_path =
      ConvertJavaStringToUTF8(env, java_update_request_path);
  WebApkInstallService::Get(profile)->UpdateAsync(
      base::FilePath(update_request_path),
      base::Bind(&OnUpdated, callback_ref));
}
