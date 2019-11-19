// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_content_bridge.h"

#include <jni.h>

#include <list>
#include <memory>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/android/chrome_jni_headers/FeedContentBridge_jni.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/core/feed_content_database.h"
#include "components/feed/core/feed_content_mutation.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

namespace feed {

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaArrayOfByteArrayToStringVector;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaRef;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfByteArray;
using base::android::ToJavaArrayOfStrings;
using base::android::JavaByteArrayToString;

static jlong JNI_FeedContentBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  FeedHostService* host_service =
      FeedHostServiceFactory::GetForBrowserContext(profile);
  DCHECK(host_service);
  FeedContentDatabase* feed_content_database =
      host_service->GetContentDatabase();
  DCHECK(feed_content_database);
  FeedContentBridge* native_content_bridge =
      new FeedContentBridge(feed_content_database);
  return reinterpret_cast<intptr_t>(native_content_bridge);
}

FeedContentBridge::FeedContentBridge(FeedContentDatabase* feed_content_database)
    : feed_content_database_(feed_content_database) {}

FeedContentBridge::~FeedContentBridge() = default;

void FeedContentBridge::Destroy(JNIEnv* env, const JavaRef<jobject>& j_this) {
  delete this;
}

void FeedContentBridge::LoadContent(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jobjectArray>& j_keys,
    const JavaRef<jobject>& j_success_callback,
    const JavaRef<jobject>& j_failure_callback) {
  std::vector<std::string> keys;
  AppendJavaStringArrayToStringVector(j_env, j_keys, &keys);
  ScopedJavaGlobalRef<jobject> success_callback(j_success_callback);
  ScopedJavaGlobalRef<jobject> failure_callback(j_failure_callback);

  feed_content_database_->LoadContent(
      keys, base::BindOnce(&FeedContentBridge::OnLoadContentDone,
                           weak_ptr_factory_.GetWeakPtr(), success_callback,
                           failure_callback));
}

void FeedContentBridge::LoadContentByPrefix(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jstring>& j_prefix,
    const JavaRef<jobject>& j_success_callback,
    const JavaRef<jobject>& j_failure_callback) {
  std::string prefix = ConvertJavaStringToUTF8(j_env, j_prefix);
  ScopedJavaGlobalRef<jobject> success_callback(j_success_callback);
  ScopedJavaGlobalRef<jobject> failure_callback(j_failure_callback);

  feed_content_database_->LoadContentByPrefix(
      prefix, base::BindOnce(&FeedContentBridge::OnLoadContentDone,
                             weak_ptr_factory_.GetWeakPtr(), success_callback,
                             failure_callback));
}

void FeedContentBridge::LoadAllContentKeys(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jobject>& j_success_callback,
    const JavaRef<jobject>& j_failure_callback) {
  ScopedJavaGlobalRef<jobject> success_callback(j_success_callback);
  ScopedJavaGlobalRef<jobject> failure_callback(j_failure_callback);

  feed_content_database_->LoadAllContentKeys(base::BindOnce(
      &FeedContentBridge::OnLoadAllContentKeysDone,
      weak_ptr_factory_.GetWeakPtr(), success_callback, failure_callback));
}

void FeedContentBridge::CommitContentMutation(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jobject>& j_callback) {
  DCHECK(content_mutation_);
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  feed_content_database_->CommitContentMutation(
      std::move(content_mutation_),
      base::BindOnce(&FeedContentBridge::OnStorageCommitDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void FeedContentBridge::CreateContentMutation(JNIEnv* j_env,
                                              const JavaRef<jobject>& j_this) {
  DCHECK(!content_mutation_);
  content_mutation_ = std::make_unique<ContentMutation>();
}

void FeedContentBridge::DeleteContentMutation(JNIEnv* j_env,
                                              const JavaRef<jobject>& j_this) {
  DCHECK(content_mutation_);
  content_mutation_.reset();
}

void FeedContentBridge::AppendDeleteOperation(JNIEnv* j_env,
                                              const JavaRef<jobject>& j_this,
                                              const JavaRef<jstring>& j_key) {
  DCHECK(content_mutation_);
  std::string key(ConvertJavaStringToUTF8(j_env, j_key));

  content_mutation_->AppendDeleteOperation(key);
}

void FeedContentBridge::AppendDeleteByPrefixOperation(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jstring>& j_prefix) {
  DCHECK(content_mutation_);
  std::string prefix(ConvertJavaStringToUTF8(j_env, j_prefix));

  content_mutation_->AppendDeleteByPrefixOperation(prefix);
}

void FeedContentBridge::AppendUpsertOperation(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jstring>& j_key,
    const JavaRef<jbyteArray>& j_data) {
  DCHECK(content_mutation_);
  std::string key(ConvertJavaStringToUTF8(j_env, j_key));
  std::string data;
  JavaByteArrayToString(j_env, j_data, &data);

  content_mutation_->AppendUpsertOperation(key, data);
}

void FeedContentBridge::AppendDeleteAllOperation(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this) {
  DCHECK(content_mutation_);

  content_mutation_->AppendDeleteAllOperation();
}

void FeedContentBridge::OnLoadContentDone(
    ScopedJavaGlobalRef<jobject> success_callback,
    ScopedJavaGlobalRef<jobject> failure_callback,
    bool success,
    std::vector<FeedContentDatabase::KeyAndData> pairs) {
  std::vector<std::string> keys;
  std::vector<std::string> data;
  for (auto pair : pairs) {
    keys.push_back(std::move(pair.first));
    data.push_back(std::move(pair.second));
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_keys = ToJavaArrayOfStrings(env, keys);
  ScopedJavaLocalRef<jobjectArray> j_data = ToJavaArrayOfByteArray(env, data);

  // Create Java Map by JNI call.
  ScopedJavaLocalRef<jobject> j_pairs =
      Java_FeedContentBridge_createKeyAndDataMap(env, j_keys, j_data);

  if (!success) {
    RunObjectCallbackAndroid(failure_callback, nullptr);
    return;
  }
  RunObjectCallbackAndroid(success_callback, j_pairs);
}

void FeedContentBridge::OnLoadAllContentKeysDone(
    ScopedJavaGlobalRef<jobject> success_callback,
    ScopedJavaGlobalRef<jobject> failure_callback,
    bool success,
    std::vector<std::string> keys) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_keys = ToJavaArrayOfStrings(env, keys);

  if (!success) {
    RunObjectCallbackAndroid(failure_callback, nullptr);
    return;
  }
  RunObjectCallbackAndroid(success_callback, j_keys);
}

void FeedContentBridge::OnStorageCommitDone(
    ScopedJavaGlobalRef<jobject> callback,
    bool success) {
  RunBooleanCallbackAndroid(callback, success);
}

}  // namespace feed
