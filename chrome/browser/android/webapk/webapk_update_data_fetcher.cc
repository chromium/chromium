// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_update_data_fetcher.h"

#include <jni.h>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/WebApkUpdateDataFetcher_jni.h"
#include "chrome/browser/android/color_helpers.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/webapk_icon_hasher.h"
#include "chrome/browser/android/webapk/webapk_web_manifest_checker.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/smhasher/src/MurmurHash2.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// Returns whether the given |url| is within the scope of the |scope| url.
bool IsInScope(const GURL& url, const GURL& scope) {
  return base::StartsWith(url.spec(), scope.spec(),
                          base::CompareCase::SENSITIVE);
}

}  // anonymous namespace

jlong JNI_WebApkUpdateDataFetcher_Initialize(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& java_scope_url,
    const JavaParamRef<jstring>& java_web_manifest_url) {
  GURL scope(base::android::ConvertJavaStringToUTF8(env, java_scope_url));
  GURL web_manifest_url(
      base::android::ConvertJavaStringToUTF8(env, java_web_manifest_url));
  WebApkUpdateDataFetcher* fetcher =
      new WebApkUpdateDataFetcher(env, obj, scope, web_manifest_url);
  return reinterpret_cast<intptr_t>(fetcher);
}

WebApkUpdateDataFetcher::WebApkUpdateDataFetcher(JNIEnv* env,
                                                 jobject obj,
                                                 const GURL& scope,
                                                 const GURL& web_manifest_url)
    : content::WebContentsObserver(nullptr),
      scope_(scope),
      web_manifest_url_(web_manifest_url),
      info_(GURL()),
      is_primary_icon_maskable_(false) {
  java_ref_.Reset(env, obj);
}

WebApkUpdateDataFetcher::~WebApkUpdateDataFetcher() {}

void WebApkUpdateDataFetcher::ReplaceWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  content::WebContentsObserver::Observe(web_contents);
}

void WebApkUpdateDataFetcher::Destroy(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  delete this;
}

void WebApkUpdateDataFetcher::Start(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  ReplaceWebContents(env, obj, java_web_contents);
  if (!web_contents()->IsLoading())
    FetchInstallableData();
}

void WebApkUpdateDataFetcher::DidStopLoading() {
  FetchInstallableData();
}

void WebApkUpdateDataFetcher::FetchInstallableData() {
  GURL url = web_contents()->GetLastCommittedURL();

  // DidStopLoading() can be called multiple times for a single URL. Only fetch
  // installable data the first time.
  if (url == last_fetched_url_)
    return;

  last_fetched_url_ = url;

  if (!IsInScope(url, scope_))
    return;

  InstallableParams params;
  params.valid_manifest = true;
  params.prefer_maskable_icon =
      ShortcutHelper::DoesAndroidSupportMaskableIcons();
  params.has_worker = true;
  params.valid_primary_icon = true;
  params.valid_badge_icon = true;
  params.wait_for_worker = true;
  InstallableManager* installable_manager =
      InstallableManager::FromWebContents(web_contents());
  installable_manager->GetData(
      params, base::BindOnce(&WebApkUpdateDataFetcher::OnDidGetInstallableData,
                             weak_ptr_factory_.GetWeakPtr()));
}

void WebApkUpdateDataFetcher::OnDidGetInstallableData(
    const InstallableData& data) {
  // Determine whether or not the manifest is WebAPK-compatible. There are 3
  // cases:
  // 1. the site isn't installable.
  // 2. the URLs in the manifest expose passwords.
  // 3. there is no manifest or the manifest is different to the one we're
  // expecting.
  // For case 3, if the manifest is empty, it means the current WebContents
  // doesn't associate with a Web Manifest. In such case, we ignore the empty
  // manifest and continue observing the WebContents's loading until we find a
  // page that links to the Web Manifest that we are looking for.
  // If the manifest URL is different from the current one, we will continue
  // observing too. It is based on our assumption that it is invalid for
  // web developers to change the Web Manifest location. When it does
  // change, we will treat the new Web Manifest as the one of another WebAPK.
  if (!data.errors.empty() || data.manifest->IsEmpty() ||
      web_manifest_url_ != data.manifest_url ||
      !AreWebManifestUrlsWebApkCompatible(*data.manifest)) {
    return;
  }

  info_.UpdateFromManifest(*data.manifest);
  info_.manifest_url = data.manifest_url;
  info_.best_primary_icon_url = data.primary_icon_url;
  primary_icon_ = *data.primary_icon;
  is_primary_icon_maskable_ = data.has_maskable_primary_icon;

  if (data.badge_icon && !data.badge_icon->drawsNothing()) {
    info_.best_badge_icon_url = data.badge_icon_url;
    badge_icon_ = *data.badge_icon;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  WebApkIconHasher::DownloadAndComputeMurmur2Hash(
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      url::Origin::Create(last_fetched_url_), info_.best_primary_icon_url,
      base::Bind(&WebApkUpdateDataFetcher::OnGotPrimaryIconMurmur2Hash,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebApkUpdateDataFetcher::OnGotPrimaryIconMurmur2Hash(
    const std::string& primary_icon_murmur2_hash) {
  if (primary_icon_murmur2_hash.empty())
    return;

  if (!info_.best_badge_icon_url.is_empty() &&
      info_.best_badge_icon_url != info_.best_primary_icon_url) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());

    WebApkIconHasher::DownloadAndComputeMurmur2Hash(
        content::BrowserContext::GetDefaultStoragePartition(profile)
            ->GetURLLoaderFactoryForBrowserProcess()
            .get(),
        url::Origin::Create(last_fetched_url_), info_.best_badge_icon_url,
        base::Bind(&WebApkUpdateDataFetcher::OnDataAvailable,
                   weak_ptr_factory_.GetWeakPtr(), primary_icon_murmur2_hash,
                   true));
  } else {
    OnDataAvailable(primary_icon_murmur2_hash, false, "");
  }
}

void WebApkUpdateDataFetcher::OnDataAvailable(
    const std::string& primary_icon_murmur2_hash,
    bool did_fetch_badge_icon,
    const std::string& badge_icon_murmur2_hash) {
  if (did_fetch_badge_icon && badge_icon_murmur2_hash.empty())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, info_.url.spec());
  ScopedJavaLocalRef<jstring> java_scope =
      base::android::ConvertUTF8ToJavaString(env, info_.scope.spec());
  ScopedJavaLocalRef<jstring> java_name =
      base::android::ConvertUTF16ToJavaString(env, info_.name);
  ScopedJavaLocalRef<jstring> java_short_name =
      base::android::ConvertUTF16ToJavaString(env, info_.short_name);
  ScopedJavaLocalRef<jstring> java_primary_icon_url =
      base::android::ConvertUTF8ToJavaString(
          env, info_.best_primary_icon_url.spec());
  ScopedJavaLocalRef<jstring> java_primary_icon_murmur2_hash =
      base::android::ConvertUTF8ToJavaString(env, primary_icon_murmur2_hash);
  ScopedJavaLocalRef<jobject> java_primary_icon =
      gfx::ConvertToJavaBitmap(&primary_icon_);
  jboolean java_is_primary_icon_maskable = is_primary_icon_maskable_;
  ScopedJavaLocalRef<jstring> java_badge_icon_url =
      base::android::ConvertUTF8ToJavaString(env,
                                             info_.best_badge_icon_url.spec());
  ScopedJavaLocalRef<jstring> java_badge_icon_murmur2_hash =
      base::android::ConvertUTF8ToJavaString(env, badge_icon_murmur2_hash);
  ScopedJavaLocalRef<jobject> java_badge_icon;
  if (!badge_icon_.drawsNothing())
    java_badge_icon = gfx::ConvertToJavaBitmap(&badge_icon_);
  ScopedJavaLocalRef<jobjectArray> java_icon_urls =
      base::android::ToJavaArrayOfStrings(env, info_.icon_urls);

  ScopedJavaLocalRef<jstring> java_share_action;
  ScopedJavaLocalRef<jstring> java_share_params_title;
  ScopedJavaLocalRef<jstring> java_share_params_text;
  ScopedJavaLocalRef<jstring> java_share_params_url;
  jboolean java_share_params_is_method_post = false;
  jboolean java_share_params_is_enctype_multipart = false;
  ScopedJavaLocalRef<jobjectArray> java_share_params_file_names;
  ScopedJavaLocalRef<jobjectArray> java_share_params_accepts;
  if (info_.share_target.has_value() && info_.share_target->action.is_valid()) {
    java_share_action = base::android::ConvertUTF8ToJavaString(
        env, info_.share_target->action.spec());
    java_share_params_title = base::android::ConvertUTF16ToJavaString(
        env, info_.share_target->params.title);
    java_share_params_text = base::android::ConvertUTF16ToJavaString(
        env, info_.share_target->params.text);

    java_share_params_is_method_post =
        (info_.share_target->method ==
         blink::Manifest::ShareTarget::Method::kPost);
    java_share_params_is_enctype_multipart =
        (info_.share_target->enctype ==
         blink::Manifest::ShareTarget::Enctype::kMultipartFormData);

    std::vector<base::string16> file_names;
    std::vector<std::vector<base::string16>> accepts;
    for (auto& f : info_.share_target->params.files) {
      file_names.push_back(f.name);
      accepts.push_back(f.accept);
    }
    java_share_params_file_names =
        base::android::ToJavaArrayOfStrings(env, file_names);
    java_share_params_accepts =
        base::android::ToJavaArrayOfStringArray(env, accepts);
  }

  Java_WebApkUpdateDataFetcher_onDataAvailable(
      env, java_ref_, java_url, java_scope, java_name, java_short_name,
      java_primary_icon_url, java_primary_icon_murmur2_hash, java_primary_icon,
      java_is_primary_icon_maskable, java_badge_icon_url,
      java_badge_icon_murmur2_hash, java_badge_icon, java_icon_urls,
      static_cast<int>(info_.display), info_.orientation,
      OptionalSkColorToJavaColor(info_.theme_color),
      OptionalSkColorToJavaColor(info_.background_color), java_share_action,
      java_share_params_title, java_share_params_text,
      java_share_params_is_method_post, java_share_params_is_enctype_multipart,
      java_share_params_file_names, java_share_params_accepts);
}
