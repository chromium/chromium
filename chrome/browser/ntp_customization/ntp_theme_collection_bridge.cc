// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_theme_collection_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/hash/hash.h"
#include "chrome/browser/ntp_customization/jni_headers/NtpThemeCollectionBridge_jni.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "components/themes/ntp_background_data.h"
#include "components/themes/ntp_background_service.h"
#include "url/android/gurl_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static jlong JNI_NtpThemeCollectionBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& j_java_obj) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  NtpThemeCollectionBridge* ntp_theme_collection_bridge =
      new NtpThemeCollectionBridge(env, profile, j_java_obj);
  return reinterpret_cast<intptr_t>(ntp_theme_collection_bridge);
}

NtpThemeCollectionBridge::NtpThemeCollectionBridge(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& j_java_obj)
    : profile_(profile),
      ntp_background_service_(
          NtpBackgroundServiceFactory::GetForProfile(profile)),
      ntp_custom_background_service_(
          NtpCustomBackgroundServiceFactory::GetForProfile(profile)),
      j_java_obj_(env, j_java_obj) {
  CHECK(ntp_background_service_);
  CHECK(ntp_custom_background_service_);
  ntp_background_service_->AddObserver(this);
  ntp_custom_background_service_->AddObserver(this);
}

void NtpThemeCollectionBridge::Destroy(JNIEnv* env) {
  if (ntp_background_service_) {
    ntp_background_service_->RemoveObserver(this);
  }
  if (ntp_custom_background_service_) {
    ntp_custom_background_service_->RemoveObserver(this);
  }
  delete this;
}

NtpThemeCollectionBridge::~NtpThemeCollectionBridge() = default;

void NtpThemeCollectionBridge::GetBackgroundCollections(
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

void NtpThemeCollectionBridge::GetBackgroundImages(
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

void NtpThemeCollectionBridge::OnCollectionInfoAvailable() {
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
    jint j_hash =
        static_cast<jint>(base::PersistentHash(collection.collection_id));
    ScopedJavaLocalRef<jobject> j_collection =
        Java_NtpThemeCollectionBridge_createCollection(env, j_id, j_label,
                                                       j_url, j_hash);
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

    ScopedJavaLocalRef<jobject> j_image =
        Java_NtpThemeCollectionBridge_createImage(
            env, j_collection_id, j_image_url, j_preview_image_url,
            j_attribution, j_attribution_url);
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
  std::optional<CustomBackground> background =
      ntp_custom_background_service_->GetCustomBackground();
  if (!background.has_value()) {
    return nullptr;
  }

  ScopedJavaLocalRef<jobject> j_url =
      url::GURLAndroid::FromNativeGURL(env, background->custom_background_url);
  ScopedJavaLocalRef<jstring> j_collection_id =
      base::android::ConvertUTF8ToJavaString(env, background->collection_id);

  return Java_NtpThemeCollectionBridge_createCustomBackgroundInfo(
      env, j_url, j_collection_id, background->is_uploaded_image,
      background->daily_refresh_enabled);
}

void NtpThemeCollectionBridge::OnCustomBackgroundImageUpdated() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NtpThemeCollectionBridge_onCustomBackgroundImageUpdated(env,
                                                               j_java_obj_);
}

void NtpThemeCollectionBridge::SetThemeCollectionImage(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_collection_id,
    const JavaParamRef<jobject>& j_image_url,
    const JavaParamRef<jobject>& j_preview_image_url,
    const JavaParamRef<jstring>& j_attribution_line_1,
    const JavaParamRef<jstring>& j_attribution_line_2,
    const JavaParamRef<jobject>& j_attribution_url) {
  if (!ntp_custom_background_service_) {
    return;
  }

  ntp_custom_background_service_->SetCustomBackgroundInfo(
      url::GURLAndroid::ToNativeGURL(env, j_image_url),
      url::GURLAndroid::ToNativeGURL(env, j_preview_image_url),
      base::android::ConvertJavaStringToUTF8(env, j_attribution_line_1),
      base::android::ConvertJavaStringToUTF8(env, j_attribution_line_2),
      url::GURLAndroid::ToNativeGURL(env, j_attribution_url),
      base::android::ConvertJavaStringToUTF8(env, j_collection_id));
}

void NtpThemeCollectionBridge::SetThemeCollectionDailyRefreshed(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_collection_id) {
  if (!ntp_custom_background_service_) {
    return;
  }

  ntp_custom_background_service_->SetCustomBackgroundInfo(
      /* background_url= */ GURL(), /* thumbnail_url= */ GURL(),
      /* attribution_line_1= */ std::string(),
      /* attribution_line_2= */ std::string(), /* action_url= */ GURL(),
      base::android::ConvertJavaStringToUTF8(env, j_collection_id));
}

void NtpThemeCollectionBridge::SelectLocalBackgroundImage(JNIEnv* env) {
  if (!ntp_custom_background_service_) {
    return;
  }

  ntp_custom_background_service_->SelectLocalBackgroundImage(base::FilePath());
}

void NtpThemeCollectionBridge::ResetCustomBackground(JNIEnv* env) {
  if (!ntp_custom_background_service_) {
    return;
  }

  ntp_custom_background_service_->ResetCustomBackgroundInfo();
}

DEFINE_JNI(NtpThemeCollectionBridge)
