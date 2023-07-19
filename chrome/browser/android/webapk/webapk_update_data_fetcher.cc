// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_update_data_fetcher.h"

#include <jni.h>
#include <set>
#include <string>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/WebApkUpdateDataFetcher_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "components/webapps/browser/android/webapp_icon.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "components/webapps/browser/android/webapps_utils.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/smhasher/src/MurmurHash2.h"
#include "ui/android/color_utils_android.h"
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

constexpr char kGotUpdateManifestHistogramName[] =
    "WebApk.Update.DidGetInstallableData";

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "WebApkUpdateManifestResult" in src/tools/metrics/histograms/enums.xml.
enum class ManifestResult {
  kDifferent = 0,
  kDifferentLegacyId = 1,
  kFound = 2,
  kMaxValue = kFound,
};

}  // anonymous namespace

jlong JNI_WebApkUpdateDataFetcher_Initialize(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& java_start_url,
    const JavaParamRef<jstring>& java_scope_url,
    const JavaParamRef<jstring>& java_web_manifest_url,
    const JavaParamRef<jstring>& java_web_manifest_id) {
  GURL start_url(base::android::ConvertJavaStringToUTF8(env, java_start_url));
  GURL scope(base::android::ConvertJavaStringToUTF8(env, java_scope_url));
  GURL web_manifest_url(
      base::android::ConvertJavaStringToUTF8(env, java_web_manifest_url));
  GURL web_manifest_id;
  if (!java_web_manifest_id.is_null()) {
    web_manifest_id =
        GURL(base::android::ConvertJavaStringToUTF8(env, java_web_manifest_id));
  }
  WebApkUpdateDataFetcher* fetcher = new WebApkUpdateDataFetcher(
      env, obj, start_url, scope, web_manifest_url, web_manifest_id);
  return reinterpret_cast<intptr_t>(fetcher);
}

WebApkUpdateDataFetcher::WebApkUpdateDataFetcher(JNIEnv* env,
                                                 jobject obj,
                                                 const GURL& start_url,
                                                 const GURL& scope,
                                                 const GURL& web_manifest_url,
                                                 const GURL& web_manifest_id)
    : content::WebContentsObserver(nullptr),
      start_url_(start_url),
      scope_(scope),
      web_manifest_url_(web_manifest_url),
      web_manifest_id_(web_manifest_id),
      info_(GURL()) {
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

  webapps::InstallableParams params;
  params.valid_manifest = true;
  params.prefer_maskable_icon =
      webapps::WebappsIconUtils::DoesAndroidSupportMaskableIcons();
  params.has_worker = false;
  params.wait_for_worker = false;
  params.valid_primary_icon = true;
  webapps::InstallableManager* installable_manager =
      webapps::InstallableManager::FromWebContents(web_contents());
  installable_manager->GetData(
      params, base::BindOnce(&WebApkUpdateDataFetcher::OnDidGetInstallableData,
                             weak_ptr_factory_.GetWeakPtr()));
}

void WebApkUpdateDataFetcher::OnDidGetInstallableData(
    const webapps::InstallableData& data) {
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
  if (!data.NoBlockingErrors() || blink::IsEmptyManifest(*data.manifest) ||
      !webapps::WebappsUtils::AreWebManifestUrlsWebApkCompatible(
          *data.manifest)) {
    return;
  }

    GURL new_manifest_id(blink::GetIdFromManifest(*data.manifest));
    if (web_manifest_id_.is_empty()) {
      // Don't have an existing manifest ID, check if either manifest URL or
      // start URL are the same. If neither of them are the same, we treat the
      // manifest as one of another WebAPK.
      if (web_manifest_url_ != *data.manifest_url &&
          start_url_ != data.manifest->start_url) {
        UMA_HISTOGRAM_ENUMERATION(kGotUpdateManifestHistogramName,
                                  ManifestResult::kDifferentLegacyId,
                                  ManifestResult::kMaxValue);
        return;
      }
    } else if (web_manifest_id_ != new_manifest_id) {
      // If the fetched manifest id is different from the current one,
      // continue observing as the id is the identity for the application. We
      // will treat the manifest with different id as the one of another WebAPK.
      UMA_HISTOGRAM_ENUMERATION(kGotUpdateManifestHistogramName,
                                ManifestResult::kDifferent,
                                ManifestResult::kMaxValue);
      return;
    }

  UMA_HISTOGRAM_ENUMERATION(kGotUpdateManifestHistogramName,
                            ManifestResult::kFound, ManifestResult::kMaxValue);

  info_.UpdateFromManifest(*data.manifest);
  info_.manifest_url = *data.manifest_url;
  info_.best_primary_icon_url = *data.primary_icon_url;
  info_.is_primary_icon_maskable = data.has_maskable_primary_icon;
  primary_icon_ = *data.primary_icon;
  info_.UpdateBestSplashIcon(*data.manifest);

  std::vector<webapps::WebappIcon> icons = info_.GetWebApkIcons();

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  webapps::WebApkIconHasher::DownloadAndComputeMurmur2Hash(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      web_contents()->GetWeakPtr(), url::Origin::Create(last_fetched_url_),
      icons,
      base::BindOnce(&WebApkUpdateDataFetcher::OnGotIconMurmur2Hashes,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkUpdateDataFetcher::OnGotIconMurmur2Hashes(
    absl::optional<std::map<std::string, webapps::WebApkIconHasher::Icon>>
        hashes) {
  if (!hashes)
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
  ScopedJavaLocalRef<jstring> java_manifest_url =
      base::android::ConvertUTF8ToJavaString(env, info_.manifest_url.spec());
  ScopedJavaLocalRef<jstring> java_manifest_id =
      base::android::ConvertUTF8ToJavaString(env, info_.manifest_id.spec());

  ScopedJavaLocalRef<jstring> java_primary_icon_url =
      base::android::ConvertUTF8ToJavaString(
          env, info_.best_primary_icon_url.spec());
  ScopedJavaLocalRef<jstring> java_primary_icon_murmur2_hash =
      base::android::ConvertUTF8ToJavaString(
          env, (*hashes)[info_.best_primary_icon_url.spec()].hash);
  ScopedJavaLocalRef<jobject> java_primary_icon =
      gfx::ConvertToJavaBitmap(primary_icon_);
  jboolean java_is_primary_icon_maskable = info_.is_primary_icon_maskable;

  ScopedJavaLocalRef<jstring> java_splash_icon_url =
      base::android::ConvertUTF8ToJavaString(env,
                                             info_.splash_image_url.spec());
  ScopedJavaLocalRef<jstring> java_splash_icon_murmur2_hash =
      base::android::ConvertUTF8ToJavaString(
          env, (*hashes)[info_.splash_image_url.spec()].hash);
  jboolean java_is_splash_icon_maskable = info_.is_splash_image_maskable;
  base::android::ScopedJavaLocalRef<jbyteArray> java_splash_icon_data =
      base::android::ToJavaByteArray(
          env, (*hashes)[info_.splash_image_url.spec()].unsafe_data);

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
         blink::mojom::ManifestShareTarget_Method::kPost);
    java_share_params_is_enctype_multipart =
        (info_.share_target->enctype ==
         blink::mojom::ManifestShareTarget_Enctype::kMultipartFormData);

    std::vector<std::u16string> file_names;
    std::vector<std::vector<std::u16string>> accepts;
    for (auto& f : info_.share_target->params.files) {
      file_names.push_back(f.name);
      accepts.push_back(f.accept);
    }
    java_share_params_file_names =
        base::android::ToJavaArrayOfStrings(env, file_names);
    java_share_params_accepts =
        base::android::ToJavaArrayOfStringArray(env, accepts);
  }

  // Wraps the shortcut info in a 2D vector for convenience.
  // The inner vector represents a shortcut items, with the following fields:
  // <name>, <short name>, <launch url>, <icon url>, <icon hash>.
  std::vector<std::vector<std::u16string>> shortcuts;
  // Each entry contains the icon data for the corresponding entry in
  // |shortcuts|.
  std::vector<std::string> shortcut_icon_data;
  DCHECK_EQ(info_.shortcut_items.size(), info_.best_shortcut_icon_urls.size());

  for (size_t i = 0; i < info_.shortcut_items.size(); i++) {
    const auto& shortcut = info_.shortcut_items[i];
    const GURL& chosen_icon_url = info_.best_shortcut_icon_urls[i];

    auto it = hashes->find(chosen_icon_url.spec());
    std::string chosen_icon_hash;
    std::string chosen_icon_data;
    if (it != hashes->end()) {
      chosen_icon_hash = it->second.hash;
      chosen_icon_data = std::move(it->second.unsafe_data);
    }

    shortcuts.push_back({shortcut.name,
                         shortcut.short_name.value_or(std::u16string()),
                         base::UTF8ToUTF16(shortcut.url.spec()),
                         base::UTF8ToUTF16(chosen_icon_url.spec()),
                         base::UTF8ToUTF16(chosen_icon_hash)});
    shortcut_icon_data.push_back(std::move(chosen_icon_data));
  }

  Java_WebApkUpdateDataFetcher_onDataAvailable(
      env, java_ref_, java_url, java_scope, java_name, java_short_name,
      java_manifest_url, java_manifest_id, java_primary_icon_url,
      java_primary_icon_murmur2_hash, java_primary_icon,
      java_is_primary_icon_maskable, java_splash_icon_url,
      java_splash_icon_murmur2_hash, java_splash_icon_data,
      java_is_splash_icon_maskable, java_icon_urls,
      static_cast<int>(info_.display), static_cast<int>(info_.orientation),
      ui::OptionalSkColorToJavaColor(info_.theme_color),
      ui::OptionalSkColorToJavaColor(info_.background_color),
      ui::OptionalSkColorToJavaColor(info_.dark_theme_color),
      ui::OptionalSkColorToJavaColor(info_.dark_background_color),
      java_share_action, java_share_params_title, java_share_params_text,
      java_share_params_is_method_post, java_share_params_is_enctype_multipart,
      java_share_params_file_names, java_share_params_accepts,
      base::android::ToJavaArrayOfStringArray(env, shortcuts),
      base::android::ToJavaArrayOfByteArray(env, shortcut_icon_data));
}
