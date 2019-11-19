// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/android/chrome_jni_headers/ExploreSitesBridgeExperimental_jni.h"
#include "chrome/android/chrome_jni_headers/ExploreSitesCategoryTile_jni.h"
#include "chrome/browser/android/explore_sites/catalog.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/ntp_json_fetcher.h"
#include "chrome/browser/android/explore_sites/url_util_experimental.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "content/public/browser/storage_partition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

namespace explore_sites {

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

constexpr char kImageFetcherUmaClientName[] = "ExploreSitesExperimental";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("explore_sites_image_fetcher", R"(
semantics {
  sender: "Explore Sites image fetcher"
  description:
    "Downloads images for explore sites usage."
  trigger:
    "When Explore Sites feature requires images from url."
  data: "Requested image at url."
  destination: GOOGLE_OWNED_SERVICE
}
policy {
  cookies_allowed: YES
  setting: "user"
  policy_exception_justification:
    "This feature is only enabled explicitly by flag."
})");

void GotNTPCategoriesFromJson(
    const ScopedJavaGlobalRef<jobject>& j_callback_ref,
    const ScopedJavaGlobalRef<jobject>& j_result_ref,
    std::unique_ptr<NTPJsonFetcher> fetcher,
    std::unique_ptr<NTPCatalog> catalog) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (catalog) {
    for (NTPCatalog::Category category : catalog->categories) {
      Java_ExploreSitesCategoryTile_createInList(
          env, j_result_ref,
          base::android::ConvertUTF8ToJavaString(env, category.id),
          base::android::ConvertUTF8ToJavaString(env, category.icon_url.spec()),
          base::android::ConvertUTF8ToJavaString(env, category.title));
    }
  }

  base::android::RunObjectCallbackAndroid(j_callback_ref, j_result_ref);
}

void OnGetIconDone(std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
                   const ScopedJavaGlobalRef<jobject>& j_callback_obj,
                   const gfx::Image& image,
                   const image_fetcher::RequestMetadata& metadata) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty()) {
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());
  }
  base::android::RunObjectCallbackAndroid(j_callback_obj, j_bitmap);

  // Delete |image_fetcher| when appropriate.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                  std::move(image_fetcher));
}

}  // namespace

// static
void JNI_ExploreSitesBridgeExperimental_GetNtpCategories(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_result_obj,
    const JavaParamRef<jobject>& j_callback_obj) {
  NTPJsonFetcher* ntp_fetcher =
      new NTPJsonFetcher(ProfileAndroid::FromProfileAndroid(j_profile));

  ntp_fetcher->Start(base::BindOnce(
      &GotNTPCategoriesFromJson, ScopedJavaGlobalRef<jobject>(j_callback_obj),
      ScopedJavaGlobalRef<jobject>(j_result_obj),
      base::WrapUnique(ntp_fetcher)));
}

// static
ScopedJavaLocalRef<jstring> JNI_ExploreSitesBridgeExperimental_GetCatalogUrl(
    JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, GetCatalogPrototypeURL().spec());
}

// static
static void JNI_ExploreSitesBridgeExperimental_GetIcon(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jobject>& j_callback_obj) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  GURL icon_url(ConvertJavaStringToUTF8(env, j_url));
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();
  image_fetcher::ImageFetcherParams params(kTrafficAnnotation,
                                           kImageFetcherUmaClientName);

  auto image_fetcher = std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<ImageDecoderImpl>(), url_loader_factory);
  // |image_fetcher| will be owned by the callback and gets destroyed at the end
  // of the callback.
  image_fetcher::ImageFetcher* image_fetcher_ptr = image_fetcher.get();
  image_fetcher_ptr->FetchImage(
      icon_url,
      base::BindOnce(&OnGetIconDone, std::move(image_fetcher),
                     ScopedJavaGlobalRef<jobject>(j_callback_obj)),
      std::move(params));
}

}  // namespace explore_sites
