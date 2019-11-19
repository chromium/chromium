// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_offline_bridge.h"

#include <memory>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/FeedOfflineBridge_jni.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/core/content_metadata.h"

using base::android::JavaRef;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace feed {

static jlong JNI_FeedOfflineBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  FeedHostService* host_service =
      FeedHostServiceFactory::GetForBrowserContext(profile);
  return reinterpret_cast<intptr_t>(
      new FeedOfflineBridge(j_this, host_service->GetOfflineHost()));
}

FeedOfflineBridge::FeedOfflineBridge(const JavaRef<jobject>& j_this,
                                     FeedOfflineHost* offline_host)
    : j_this_(ScopedJavaGlobalRef<jobject>(j_this)),
      offline_host_(offline_host) {
  DCHECK(offline_host_);
  // The host guarantees to not invoke these callbacks until Initialize() exits.
  // This is important because until the Java bridge's constructor finishes, any
  // attempt to cross JNI from native to Java will fail.
  offline_host_->Initialize(
      base::BindRepeating(&FeedOfflineBridge::TriggerGetKnownContent,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&FeedOfflineBridge::NotifyStatusChange,
                          weak_factory_.GetWeakPtr()));
}

FeedOfflineBridge::~FeedOfflineBridge() = default;

void FeedOfflineBridge::Destroy(JNIEnv* env, const JavaRef<jobject>& j_this) {
  delete this;
}

ScopedJavaLocalRef<jobject> FeedOfflineBridge::GetOfflineId(
    JNIEnv* env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jstring>& j_url) {
  std::string url = ConvertJavaStringToUTF8(env, j_url);
  base::Optional<int64_t> id = offline_host_->GetOfflineId(url);
  return id ? Java_FeedOfflineBridge_createLong(env, *id) : nullptr;
}

void FeedOfflineBridge::GetOfflineStatus(JNIEnv* env,
                                         const JavaRef<jobject>& j_this,
                                         const JavaRef<jobjectArray>& j_urls,
                                         const JavaRef<jobject>& j_callback) {
  std::vector<std::string> urls;
  base::android::AppendJavaStringArrayToStringVector(env, j_urls, &urls);
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  offline_host_->GetOfflineStatus(
      std::move(urls), base::BindOnce(&FeedOfflineBridge::OnGetOfflineStatus,
                                      weak_factory_.GetWeakPtr(), callback));
}

void FeedOfflineBridge::OnContentRemoved(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jobjectArray>& j_urls) {
  std::vector<std::string> urls;
  base::android::AppendJavaStringArrayToStringVector(env, j_urls, &urls);
  offline_host_->OnContentRemoved(urls);
}

void FeedOfflineBridge::OnNewContentReceived(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_this) {
  offline_host_->OnNewContentReceived();
}

void FeedOfflineBridge::OnNoListeners(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_this) {
  offline_host_->OnNoListeners();
}

void FeedOfflineBridge::AppendContentMetadata(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_this,
    const base::android::JavaRef<jstring>& j_url,
    const base::android::JavaRef<jstring>& j_title,
    const jlong j_time_published_ms,
    const base::android::JavaRef<jstring>& j_image_url,
    const base::android::JavaRef<jstring>& j_publisher,
    const base::android::JavaRef<jstring>& j_favicon_url,
    const base::android::JavaRef<jstring>& j_snippet) {
  ContentMetadata metadata;
  DCHECK(!j_url.is_null());
  metadata.url = base::android::ConvertJavaStringToUTF8(env, j_url);
  DCHECK(!j_title.is_null());
  metadata.title = base::android::ConvertJavaStringToUTF8(env, j_title);
  metadata.time_published = base::Time::FromJavaTime(j_time_published_ms);
  if (!j_image_url.is_null()) {
    metadata.image_url =
        base::android::ConvertJavaStringToUTF8(env, j_image_url);
  }
  if (!j_publisher.is_null()) {
    metadata.publisher =
        base::android::ConvertJavaStringToUTF8(env, j_publisher);
  }
  if (!j_favicon_url.is_null()) {
    metadata.favicon_url =
        base::android::ConvertJavaStringToUTF8(env, j_favicon_url);
  }
  if (!j_snippet.is_null()) {
    metadata.snippet = base::android::ConvertJavaStringToUTF8(env, j_snippet);
  }
  known_content_metadata_buffer_.emplace_back(std::move(metadata));
}

void FeedOfflineBridge::OnGetKnownContentDone(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_this) {
  offline_host_->OnGetKnownContentDone(
      std::move(known_content_metadata_buffer_));
}

void FeedOfflineBridge::TriggerGetKnownContent() {
  DCHECK(known_content_metadata_buffer_.empty());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedOfflineBridge_getKnownContent(env, j_this_);
}

void FeedOfflineBridge::NotifyStatusChange(const std::string& url,
                                           bool available_offline) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_string =
      base::android::ConvertUTF8ToJavaString(env, url);
  Java_FeedOfflineBridge_notifyStatusChange(env, j_this_, j_string,
                                            available_offline);
}

void FeedOfflineBridge::OnGetOfflineStatus(
    ScopedJavaGlobalRef<jobject> callback,
    std::vector<std::string> urls) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_urls =
      base::android::ToJavaArrayOfStrings(env, urls);
  RunObjectCallbackAndroid(callback, j_urls);
}

}  // namespace feed
