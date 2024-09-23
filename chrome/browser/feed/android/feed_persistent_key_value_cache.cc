// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/persistent_key_value_store.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/feed/android/jni_headers/FeedPersistentKeyValueCache_jni.h"

namespace feed {
namespace {
using base::android::JavaParamRef;

std::string JavaByteArrayToString(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& byte_array) {
  std::string result;
  base::android::JavaByteArrayToString(env, byte_array, &result);
  return result;
}

void OnLookupFinished(JNIEnv* env,
                      base::android::ScopedJavaGlobalRef<jobject> callback,
                      PersistentKeyValueStore::Result result) {
  base::android::ScopedJavaLocalRef<jbyteArray> j_result;
  if (result.get_result) {
    j_result = base::android::ToJavaByteArray(env, *result.get_result);
  }
  base::android::RunObjectCallbackAndroid(callback, j_result);
}

void CallRunnable(base::android::ScopedJavaGlobalRef<jobject> runnable,
                  PersistentKeyValueStore::Result result) {
  if (runnable)
    base::android::RunRunnableAndroid(runnable);
}

PersistentKeyValueStore* GetStore() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return nullptr;

  FeedService* feed_service = FeedServiceFactory::GetForBrowserContext(profile);
  if (!feed_service)
    return nullptr;

  return &feed_service->GetStream()->GetPersistentKeyValueStore();
}

}  // namespace

void JNI_FeedPersistentKeyValueCache_Lookup(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& j_key,
    const JavaParamRef<jobject>& j_response_callback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(j_response_callback);

  PersistentKeyValueStore* store = GetStore();
  if (!store) {
    OnLookupFinished(env, std::move(callback), {});
    return;
  }
  return store->Get(
      JavaByteArrayToString(env, j_key),
      base::BindOnce(&OnLookupFinished, env, std::move(callback)));
}

void JNI_FeedPersistentKeyValueCache_Put(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& j_key,
    const JavaParamRef<jbyteArray>& j_value,
    const JavaParamRef<jobject>& j_runnable) {
  base::android::ScopedJavaGlobalRef<jobject> callback(j_runnable);

  PersistentKeyValueStore* store = GetStore();
  if (!store) {
    base::android::RunRunnableAndroid(j_runnable);
    return;
  }
  return store->Put(
      JavaByteArrayToString(env, j_key), JavaByteArrayToString(env, j_value),
      base::BindOnce(&CallRunnable,
                     base::android::ScopedJavaGlobalRef<jobject>(j_runnable)));
}

void JNI_FeedPersistentKeyValueCache_Evict(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& j_key,
    const JavaParamRef<jobject>& j_runnable) {
  base::android::ScopedJavaGlobalRef<jobject> callback(j_runnable);

  PersistentKeyValueStore* store = GetStore();
  if (!store) {
    base::android::RunRunnableAndroid(j_runnable);
    return;
  }
  return store->Delete(
      JavaByteArrayToString(env, j_key),
      base::BindOnce(&CallRunnable,
                     base::android::ScopedJavaGlobalRef<jobject>(j_runnable)));
}

}  // namespace feed
