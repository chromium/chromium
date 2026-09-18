// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_theme_collection_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/hash/hash.h"
#include "chrome/browser/ntp_customization/ntp_android_background_service_factory.h"
#include "chrome/browser/ntp_customization/ntp_android_custom_background_service.h"
#include "chrome/browser/ntp_customization/ntp_android_custom_background_service_factory.h"
#include "chrome/browser/ntp_customization/ntp_customization_utils.h"
#include "components/themes/ntp_background_data.h"
#include "components/themes/ntp_background_service.h"
#include "third_party/jni_zero/default_conversions.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ntp_customization/jni_headers/NtpThemeCollectionBridge_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

static int64_t JNI_NtpThemeCollectionBridge_Init(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile,
    const JavaRef<jobject>& j_java_obj) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  NtpThemeCollectionBridge* ntp_theme_collection_bridge =
      new NtpThemeCollectionBridge(env, profile, j_java_obj);
  return reinterpret_cast<intptr_t>(ntp_theme_collection_bridge);
}

NtpThemeCollectionBridge::NtpThemeCollectionBridge(
    JNIEnv* env,
    Profile* profile,
    const JavaRef<jobject>& j_java_obj)
    : profile_(profile),
      ntp_background_service_(
          NtpAndroidBackgroundServiceFactory::GetForProfile(profile)),
      ntp_custom_background_service_(
          NtpAndroidCustomBackgroundServiceFactory::GetForProfile(profile)),
      j_java_obj_(env, j_java_obj) {
  CHECK(ntp_background_service_);
  CHECK(ntp_custom_background_service_);
  ntp_background_service_->AddObserver(this);
  ntp_custom_background_service_->SetThemeCollectionBridge(this);
}

void NtpThemeCollectionBridge::Destroy(JNIEnv* env) {
  if (ntp_background_service_) {
    ntp_background_service_->RemoveObserver(this);
  }
  if (ntp_custom_background_service_) {
    ntp_custom_background_service_->SetThemeCollectionBridge(nullptr);
  }
  delete this;
}

void NtpThemeCollectionBridge::DisconnectCustomBackgroundService() {
  ntp_custom_background_service_ = nullptr;
}

NtpThemeCollectionBridge::NtpThemeCollectionBridge() = default;
NtpThemeCollectionBridge::~NtpThemeCollectionBridge() = default;

void NtpThemeCollectionBridge::GetBackgroundCollections(
    JNIEnv* env,
    const JavaRef<jobject>& j_callback) {
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

void NtpThemeCollectionBridge::GetBackgroundImages(
    const std::string& collection_id,
    const JavaRef<jobject>& j_callback) {
  if (j_background_images_callback_) {
    base::android::RunObjectCallbackAndroid(j_background_images_callback_,
                                            nullptr);
  }

  if (!ntp_background_service_) {
    base::android::RunObjectCallbackAndroid(j_callback, nullptr);
    return;
  }

  j_background_images_callback_.Reset(j_callback);
  ntp_background_service_->FetchCollectionImageInfo(collection_id);
}

void NtpThemeCollectionBridge::OnCollectionInfoAvailable() {
  if (!j_background_collections_callback_) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<ScopedJavaLocalRef<jobject>> j_collections;

  for (const auto& collection : ntp_background_service_->collection_info()) {
    ScopedJavaLocalRef<jobject> j_collection =
        Java_NtpThemeCollectionBridge_createCollection(
            env, collection.collection_id, collection.collection_name,
            collection.preview_image_url,
            static_cast<int32_t>(
                base::PersistentHash(collection.collection_id)));
    j_collections.push_back(j_collection);
  }

  base::android::RunObjectCallbackAndroid(
      j_background_collections_callback_,
      base::android::ToJavaArrayOfObjects(env, j_collections));
  j_background_collections_callback_.Reset();
}

void NtpThemeCollectionBridge::OnCollectionImagesAvailable() {
  if (!j_background_images_callback_) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<ScopedJavaLocalRef<jobject>> j_images;

  for (const auto& image : ntp_background_service_->collection_images()) {
    ScopedJavaLocalRef<jobject> j_image =
        Java_NtpThemeCollectionBridge_createImage(
            env, image.collection_id, image.image_url,
            image.thumbnail_image_url, image.attribution,
            image.attribution_action_url);
    j_images.push_back(j_image);
  }

  base::android::RunObjectCallbackAndroid(
      j_background_images_callback_,
      base::android::ToJavaArrayOfObjects(env, j_images));
  j_background_images_callback_.Reset();
}

void NtpThemeCollectionBridge::OnNextCollectionImageAvailable() {}

void NtpThemeCollectionBridge::OnNtpBackgroundServiceShuttingDown() {
  ntp_background_service_->RemoveObserver(this);
  ntp_background_service_ = nullptr;
}

ScopedJavaLocalRef<jobject> NtpThemeCollectionBridge::GetCustomBackgroundInfo(
    JNIEnv* env) {
  if (!ntp_custom_background_service_) {
    return nullptr;
  }
  std::optional<CustomBackground> background =
      ntp_custom_background_service_->GetCustomBackground();
  if (!background.has_value()) {
    return nullptr;
  }

  return Java_NtpThemeCollectionBridge_createCustomBackgroundInfo(
      env, background->custom_background_url, background->collection_id,
      background->is_uploaded_image, background->daily_refresh_enabled,
      ntp_customization::GetCustomBackgroundAttribution(*background));
}

void NtpThemeCollectionBridge::OnCustomBackgroundImageUpdated() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NtpThemeCollectionBridge_onCustomBackgroundImageUpdated(env,
                                                               j_java_obj_);
}

void NtpThemeCollectionBridge::SetThemeCollectionImage(
    const std::string& collection_id,
    const GURL& image_url,
    const GURL& preview_image_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& attribution_url) {
  if (!ntp_custom_background_service_) {
    return;
  }

  ntp_custom_background_service_->SetCustomBackgroundInfo(
      image_url, preview_image_url, attribution_line_1, attribution_line_2,
      attribution_url, collection_id);
}

void NtpThemeCollectionBridge::SetThemeCollectionDailyRefreshed(
    const std::string& collection_id) {
  if (!ntp_custom_background_service_) {
    return;
  }

  ntp_custom_background_service_->SetCustomBackgroundInfo(
      /* background_url= */ GURL(), /* thumbnail_url= */ GURL(),
      /* attribution_line_1= */ std::string(),
      /* attribution_line_2= */ std::string(), /* action_url= */ GURL(),
      collection_id);
}

void NtpThemeCollectionBridge::FetchNextThemeCollectionImage(JNIEnv* env) {
  if (!ntp_custom_background_service_) {
    return;
  }
  ntp_custom_background_service_->RefreshBackgroundIfNeeded();
}


DEFINE_JNI(NtpThemeCollectionBridge)
