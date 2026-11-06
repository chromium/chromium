// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_STORAGE_LOADED_DATA_ANDROID_H_
#define CHROME_BROWSER_ANDROID_STORAGE_LOADED_DATA_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/storage_loading_context.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "third_party/jni_zero/jni_zero.h"

namespace tabs {

// Android wrapper for StorageLoadedData.
class StorageLoadedDataAndroid {
 public:
  ~StorageLoadedDataAndroid();

  StorageLoadedDataAndroid(const StorageLoadedDataAndroid&) = delete;
  StorageLoadedDataAndroid& operator=(const StorageLoadedDataAndroid&) = delete;

  // Marshals `data` into a new Java `StorageLoadedData` owning a
  // `StorageLoadedDataAndroid` peer via `JniUniquePtr`.
  static base::android::ScopedJavaLocalRef<jobject> CreateJavaObject(
      JNIEnv* env,
      std::unique_ptr<StorageLoadedData> data);

  // Destroys the Java StorageLoadedData (and its owned C++ peer) if an
  // in-flight background load is cancelled before reaching Java.
  static void DestroyJavaObject(const jni_zero::JavaRef<jobject>& j_object);

  // Note: loaded_tabs and loaded_groups are moved out during
  // CreateJavaObject(); only loading context and node metadata remain
  // populated.
  StorageLoadedData* GetData() const;

  static StorageLoadedDataAndroid* FromJavaObject(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& obj);

 private:
  explicit StorageLoadedDataAndroid(std::unique_ptr<StorageLoadedData> data);

  std::unique_ptr<StorageLoadedData> data_;
};

base::android::ScopedJavaLocalRef<jobject> CreateLoadedTabState(
    JNIEnv* env,
    tabs_pb::TabState& tab_state);

base::android::ScopedJavaLocalRef<jobject> CreateStorageLoadWarning(
    JNIEnv* env,
    const StorageLoadingContext::Warning& warning);

base::android::ScopedJavaLocalRef<jobject> CreateTabGroupCollectionData(
    JNIEnv* env,
    const TabGroupCollectionData& data);

}  // namespace tabs

namespace jni_zero {

template <>
inline ScopedJavaLocalRef<jobject>
ToJniType<tabs::StorageLoadingContext::Warning>(
    JNIEnv* env,
    const tabs::StorageLoadingContext::Warning& input) {
  return tabs::CreateStorageLoadWarning(env, input);
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<tabs::TabGroupCollectionData>(
    JNIEnv* env,
    const tabs::TabGroupCollectionData& input) {
  return tabs::CreateTabGroupCollectionData(env, input);
}

template <>
inline ScopedJavaLocalRef<jobject>
ToJniType<std::unique_ptr<tabs::TabGroupCollectionData>>(
    JNIEnv* env,
    const std::unique_ptr<tabs::TabGroupCollectionData>& input) {
  return input ? ToJniType(env, *input) : nullptr;
}

// TODO(469809169): Rather than using a const_cast, declare the function as
// tabs_pb::TabState&& and having our codegen use std::move().
template <>
inline ScopedJavaLocalRef<jobject> ToJniType<tabs_pb::TabState>(
    JNIEnv* env,
    const tabs_pb::TabState& input) {
  return tabs::CreateLoadedTabState(env, const_cast<tabs_pb::TabState&>(input));
}

template <>
inline tabs::StorageLoadedData* FromJniType<tabs::StorageLoadedData*>(
    JNIEnv* env,
    const JavaRef<jobject>& j_storage_loaded_data) {
  if (!j_storage_loaded_data) {
    return nullptr;
  }
  tabs::StorageLoadedDataAndroid* android_data =
      tabs::StorageLoadedDataAndroid::FromJavaObject(env,
                                                     j_storage_loaded_data);
  return android_data ? android_data->GetData() : nullptr;
}

}  // namespace jni_zero

#endif  // CHROME_BROWSER_ANDROID_STORAGE_LOADED_DATA_ANDROID_H_
