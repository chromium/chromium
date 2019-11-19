// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_IMAGE_FETCHER_IMAGE_FETCHER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_IMAGE_FETCHER_IMAGE_FETCHER_BRIDGE_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "ui/gfx/image/image.h"

namespace image_fetcher {

class ImageFetcherService;

// Native counterpart of ImageFetcherBridge.java.
class ImageFetcherBridge {
 public:
  ImageFetcherBridge(ImageFetcherService* image_fetcher_service,
                     base::FilePath base_file_path);
  ~ImageFetcherBridge();

  void Destroy(JNIEnv* j_env, const base::android::JavaRef<jobject>& j_this);

  base::android::ScopedJavaLocalRef<jstring> GetFilePath(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jstring>& j_url);

  void FetchImageData(JNIEnv* j_env,
                      const base::android::JavaRef<jobject>& j_this,
                      const jint j_image_fetcher_config,
                      const base::android::JavaRef<jstring>& j_url,
                      const base::android::JavaRef<jstring>& j_client_name,
                      const base::android::JavaRef<jobject>& j_callback);

  void FetchImage(JNIEnv* j_env,
                  const base::android::JavaRef<jobject>& j_this,
                  const jint j_image_fetcher_config,
                  const base::android::JavaRef<jstring>& j_url,
                  const base::android::JavaRef<jstring>& j_client_name,
                  const base::android::JavaRef<jobject>& j_callback);

  void ReportEvent(JNIEnv* j_env,
                   const base::android::JavaRef<jobject>& j_this,
                   const base::android::JavaRef<jstring>& j_client_name,
                   const jint j_event_id);

  void ReportCacheHitTime(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this,
                          const base::android::JavaRef<jstring>& j_client_name,
                          const jlong start_time_millis);

  void ReportTotalFetchTimeFromNative(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jstring>& j_client_name,
      const jlong start_time_millis);

 private:
  void OnImageDataFetched(base::android::ScopedJavaGlobalRef<jobject> callback,
                          const std::string& image_data,
                          const RequestMetadata& request_metadata);

  void OnImageFetched(base::android::ScopedJavaGlobalRef<jobject> callback,
                      const gfx::Image& image,
                      const RequestMetadata& request_metadata);

  // This service outlives the bridge.
  ImageFetcherService* image_fetcher_service_;
  base::FilePath base_file_path_;

  base::WeakPtrFactory<ImageFetcherBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherBridge);
};

}  // namespace image_fetcher

#endif  // CHROME_BROWSER_ANDROID_IMAGE_FETCHER_IMAGE_FETCHER_BRIDGE_H_
