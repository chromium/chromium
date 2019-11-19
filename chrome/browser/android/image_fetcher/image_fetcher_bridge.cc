// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/image_fetcher/image_fetcher_bridge.h"

#include <jni.h>

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/image_fetcher/jni_headers/ImageFetcherBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace image_fetcher {

namespace {

// Keep in sync with postfix found in image_data_store_disk.cc.
const base::FilePath::CharType kPathPostfix[] =
    FILE_PATH_LITERAL("image_data_storage");

// TODO(wylieb): Allow java clients to map to a traffic_annotation here.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cached_image_fetcher", R"(
        semantics {
          sender: "Cached Image Fetcher Fetch"
          description:
            "Fetches and caches images for Chrome features."
          trigger:
            "Triggered when a feature requests an image fetch."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Cache can be cleared through settings."
        policy_exception_justification:
          "This feature allows many different Chrome features to fetch/cache "
          "images and thus there is no Chrome-wide policy to disable it."
      })");

}  // namespace

// static
jlong JNI_ImageFetcherBridge_Init(JNIEnv* j_env,
                                  const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  SimpleFactoryKey* simple_factory_key = profile->GetProfileKey();
  base::FilePath file_path =
      ImageFetcherServiceFactory::GetCachePath(simple_factory_key)
          .Append(kPathPostfix);

  ImageFetcherService* if_service =
      ImageFetcherServiceFactory::GetForKey(simple_factory_key);

  ImageFetcherBridge* native_if_bridge =
      new ImageFetcherBridge(if_service, file_path);
  return reinterpret_cast<intptr_t>(native_if_bridge);
}

ImageFetcherBridge::ImageFetcherBridge(
    ImageFetcherService* image_fetcher_service,
    base::FilePath base_file_path)
    : image_fetcher_service_(image_fetcher_service),
      base_file_path_(base_file_path) {}

ImageFetcherBridge::~ImageFetcherBridge() = default;

void ImageFetcherBridge::Destroy(JNIEnv* j_env,
                                 const JavaRef<jobject>& j_this) {
  delete this;
}

ScopedJavaLocalRef<jstring> ImageFetcherBridge::GetFilePath(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jstring>& j_url) {
  std::string url = base::android::ConvertJavaStringToUTF8(j_url);
  std::string file_path =
      base_file_path_.Append(ImageCache::HashUrlToKey(url)).MaybeAsASCII();
  return base::android::ConvertUTF8ToJavaString(j_env, file_path);
}

void ImageFetcherBridge::FetchImageData(JNIEnv* j_env,
                                        const JavaRef<jobject>& j_this,
                                        const jint j_image_fetcher_config,
                                        const JavaRef<jstring>& j_url,
                                        const JavaRef<jstring>& j_client_name,
                                        const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  ImageFetcherConfig config =
      static_cast<ImageFetcherConfig>(j_image_fetcher_config);
  std::string url = base::android::ConvertJavaStringToUTF8(j_url);
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);

  image_fetcher::ImageFetcherParams params(kTrafficAnnotation, client_name);

  // We can skip transcoding here because this method is used in java as
  // ImageFetcher.fetchGif, which decodes the data in a Java-only library.
  params.set_skip_transcoding(true);
  image_fetcher_service_->GetImageFetcher(config)->FetchImageData(
      GURL(url),
      base::BindOnce(&ImageFetcherBridge::OnImageDataFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback),
      std::move(params));
}

void ImageFetcherBridge::FetchImage(JNIEnv* j_env,
                                    const JavaRef<jobject>& j_this,
                                    const jint j_image_fetcher_config,
                                    const JavaRef<jstring>& j_url,
                                    const JavaRef<jstring>& j_client_name,
                                    const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  ImageFetcherConfig config =
      static_cast<ImageFetcherConfig>(j_image_fetcher_config);
  std::string url = base::android::ConvertJavaStringToUTF8(j_url);
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);

  ImageFetcherParams params(kTrafficAnnotation, client_name);
  image_fetcher_service_->GetImageFetcher(config)->FetchImage(
      GURL(url),
      base::BindOnce(&ImageFetcherBridge::OnImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback),
      std::move(params));
}

void ImageFetcherBridge::ReportEvent(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_client_name,
    const jint j_event_id) {
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);
  ImageFetcherEvent event = static_cast<ImageFetcherEvent>(j_event_id);
  ImageFetcherMetricsReporter::ReportEvent(client_name, event);
}

void ImageFetcherBridge::ReportCacheHitTime(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_client_name,
    const jlong start_time_millis) {
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);
  base::Time start_time = base::Time::FromJavaTime(start_time_millis);
  ImageFetcherMetricsReporter::ReportImageLoadFromCacheTimeJava(client_name,
                                                                start_time);
}

void ImageFetcherBridge::ReportTotalFetchTimeFromNative(
    JNIEnv* j_env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_client_name,
    const jlong start_time_millis) {
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);
  base::Time start_time = base::Time::FromJavaTime(start_time_millis);
  ImageFetcherMetricsReporter::ReportTotalFetchFromNativeTimeJava(client_name,
                                                                  start_time);
}

void ImageFetcherBridge::OnImageDataFetched(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const std::string& image_data,
    const RequestMetadata& request_metadata) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_bytes = base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(image_data.data()),
      image_data.size());
  RunObjectCallbackAndroid(callback, j_bytes);
}

void ImageFetcherBridge::OnImageFetched(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const gfx::Image& image,
    const RequestMetadata& request_metadata) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty()) {
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());
  }
  RunObjectCallbackAndroid(callback, j_bitmap);
}

}  // namespace image_fetcher
