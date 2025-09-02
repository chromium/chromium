// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_theme_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/ntp_customization/jni_headers/NtpThemeBridge_jni.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "components/themes/ntp_background_data.h"
#include "components/themes/ntp_background_service.h"
#include "url/android/gurl_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static jlong JNI_NtpThemeBridge_Init(JNIEnv* env,
                                     const JavaParamRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  NtpThemeBridge* ntp_theme_bridge = new NtpThemeBridge(profile);
  return reinterpret_cast<intptr_t>(ntp_theme_bridge);
}

NtpThemeBridge::NtpThemeBridge(Profile* profile)
    : profile_(profile),
      ntp_background_service_(
          NtpBackgroundServiceFactory::GetForProfile(profile)) {
  CHECK(ntp_background_service_);
  ntp_background_service_->AddObserver(this);
}

void NtpThemeBridge::Destroy(JNIEnv* env) {
  if (ntp_background_service_) {
    ntp_background_service_->RemoveObserver(this);
  }
  delete this;
}

NtpThemeBridge::~NtpThemeBridge() = default;

void NtpThemeBridge::GetBackgroundCollections(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_callback) {
  if (j_background_collections_callback_) {
    base::android::RunObjectCallbackAndroid(j_background_collections_callback_,
                                            nullptr);
  }

  if (!ntp_background_service_) {
    base::android::RunObjectCallbackAndroid(j_callback, nullptr);
    return;
  }

  j_background_collections_callback_.Reset(j_callback);
  ntp_background_service_->FetchCollectionInfo();
}

void NtpThemeBridge::GetBackgroundImages(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_collection_id,
    const JavaParamRef<jobject>& j_callback) {
  if (j_background_images_callback_) {
    base::android::RunObjectCallbackAndroid(j_background_images_callback_,
                                            nullptr);
  }

  if (!ntp_background_service_) {
    base::android::RunObjectCallbackAndroid(j_callback, nullptr);
    return;
  }

  j_background_images_callback_.Reset(j_callback);
  ntp_background_service_->FetchCollectionImageInfo(
      base::android::ConvertJavaStringToUTF8(env, j_collection_id));
}

void NtpThemeBridge::OnCollectionInfoAvailable() {
  if (!j_background_collections_callback_) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<ScopedJavaLocalRef<jobject>> j_collections;

  for (const auto& collection : ntp_background_service_->collection_info()) {
    ScopedJavaLocalRef<jstring> j_id =
        base::android::ConvertUTF8ToJavaString(env, collection.collection_id);
    ScopedJavaLocalRef<jstring> j_label =
        base::android::ConvertUTF8ToJavaString(env, collection.collection_name);
    ScopedJavaLocalRef<jobject> j_url =
        url::GURLAndroid::FromNativeGURL(env, collection.preview_image_url);
    ScopedJavaLocalRef<jobject> j_collection =
        Java_NtpThemeBridge_createCollection(env, j_id, j_label, j_url);
    j_collections.push_back(j_collection);
  }

  base::android::RunObjectCallbackAndroid(
      j_background_collections_callback_,
      base::android::ToJavaArrayOfObjects(env, j_collections));
  j_background_collections_callback_.Reset();
}

void NtpThemeBridge::OnCollectionImagesAvailable() {
  if (!j_background_images_callback_) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<ScopedJavaLocalRef<jobject>> j_images;

  for (const auto& image : ntp_background_service_->collection_images()) {
    ScopedJavaLocalRef<jstring> j_collection_id =
        base::android::ConvertUTF8ToJavaString(env, image.collection_id);
    ScopedJavaLocalRef<jobject> j_image_url =
        url::GURLAndroid::FromNativeGURL(env, image.image_url);
    ScopedJavaLocalRef<jobject> j_preview_image_url =
        url::GURLAndroid::FromNativeGURL(env, image.thumbnail_image_url);
    ScopedJavaLocalRef<jobjectArray> j_attribution =
        base::android::ToJavaArrayOfStrings(env, image.attribution);
    ScopedJavaLocalRef<jobject> j_attribution_url =
        url::GURLAndroid::FromNativeGURL(env, image.attribution_action_url);

    ScopedJavaLocalRef<jobject> j_image = Java_NtpThemeBridge_createImage(
        env, j_collection_id, j_image_url, j_preview_image_url, j_attribution,
        j_attribution_url);
    j_images.push_back(j_image);
  }

  base::android::RunObjectCallbackAndroid(
      j_background_images_callback_,
      base::android::ToJavaArrayOfObjects(env, j_images));
  j_background_images_callback_.Reset();
}

void NtpThemeBridge::OnNextCollectionImageAvailable() {}

void NtpThemeBridge::OnNtpBackgroundServiceShuttingDown() {
  ntp_background_service_->RemoveObserver(this);
  ntp_background_service_ = nullptr;
}
