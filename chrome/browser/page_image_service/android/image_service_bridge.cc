// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_image_service/android/image_service_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/page_image_service/metrics_util.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/page_image_service/android/jni_headers/ImageServiceBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

// Handles the response from page_image_service::ImageService when requesting
// a salient image url.
void HandleImageUrlResponse(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const GURL& image_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      callback, url::GURLAndroid::FromNativeGURL(env, image_url));
}

}  // namespace

static jlong JNI_ImageServiceBridge_Init(JNIEnv* env, Profile* profile) {
  DCHECK(!profile->IsOffTheRecord());
  ImageServiceBridge* image_service_bridge = new ImageServiceBridge(
      page_image_service::ImageServiceFactory::GetForBrowserContext(profile),
      IdentityManagerFactory::GetForProfile(profile));
  return reinterpret_cast<intptr_t>(image_service_bridge);
}

static std::string JNI_ImageServiceBridge_ClientIdToString(
    JNIEnv* env,
    const jint client_id) {
  return page_image_service::ClientIdToString(
      static_cast<page_image_service::mojom::ClientId>(client_id));
}

ImageServiceBridge::ImageServiceBridge(
    page_image_service::ImageService* image_service,
    signin::IdentityManager* identity_manager)
    : image_service_(image_service), identity_manager_(identity_manager) {}

ImageServiceBridge::~ImageServiceBridge() = default;

void ImageServiceBridge::Destroy(JNIEnv* env) {
  delete this;
}

void ImageServiceBridge::FetchImageUrlFor(
    JNIEnv* env,
    const bool is_account_data,
    const jint client_id,
    const GURL& page_url,
    const JavaParamRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  FetchImageUrlForImpl(
      is_account_data,
      static_cast<page_image_service::mojom::ClientId>(client_id), page_url,
      base::BindOnce(&HandleImageUrlResponse, callback));
}

void ImageServiceBridge::FetchImageUrlForImpl(
    const bool is_account_data,
    const page_image_service::mojom::ClientId client_id,
    const GURL& page_url,
    page_image_service::ImageService::ResultCallback callback) {
  if (!page_url.is_valid()) {
    std::move(callback).Run(GURL());
    return;
  }

  // The caller must either be (1) syncing or (2) the underlying data-type
  // being fetched for is account-bound. If neither of these conditions are
  // met, then return early with an empty result.
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
      !is_account_data) {
    std::move(callback).Run(GURL());
    return;
  }
  image_service_->FetchImageFor(client_id, page_url,
                                page_image_service::mojom::Options(),
                                std::move(callback));

}  // namespace page_image_service
