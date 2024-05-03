// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_IMAGE_SERVICE_ANDROID_IMAGE_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_PAGE_IMAGE_SERVICE_ANDROID_IMAGE_SERVICE_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_image_service/image_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/android/gurl_android.h"

// Provides the native implementation of the java bridge. Allowing java code to
// access ImageService.
class ImageServiceBridge {
 public:
  ImageServiceBridge(page_image_service::ImageService* image_service,
                     signin::IdentityManager* identity_manager);
  ImageServiceBridge(const ImageServiceBridge&) = delete;
  ImageServiceBridge& operator=(const ImageServiceBridge&) = delete;

  ~ImageServiceBridge();

  void Destroy(JNIEnv* env);

  // Fetches an image url for the given `page_url`. This request is only made
  // if there's a syncing consent level for the primary account or the
  // underlying datatype is account-bound, `is_account_data`.
  void FetchImageUrlFor(JNIEnv* env,
                        const bool is_account_data,
                        const jint client_id,
                        const GURL& page_url,
                        const base::android::JavaParamRef<jobject>& j_callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(ImageServiceBridgeTest, TestGetImageUrl);
  FRIEND_TEST_ALL_PREFIXES(ImageServiceBridgeTest,
                           TestGetImageUrlWithInvalidURL);

  void FetchImageUrlForImpl(
      const bool is_account_data,
      const page_image_service::mojom::ClientId client_id,
      const GURL& page_url,
      page_image_service::ImageService::ResultCallback callback);

  const raw_ptr<page_image_service::ImageService> image_service_;  // weak
  const raw_ptr<signin::IdentityManager> identity_manager_;        // weak
};

#endif  // CHROME_BROWSER_PAGE_IMAGE_SERVICE_ANDROID_IMAGE_SERVICE_BRIDGE_H_
