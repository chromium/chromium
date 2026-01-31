// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_STORAGE_LOADED_DATA_ANDROID_H_
#define CHROME_BROWSER_ANDROID_STORAGE_LOADED_DATA_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "third_party/jni_zero/jni_zero.h"

namespace tabs {

// Android wrapper for StorageLoadedData.
class StorageLoadedDataAndroid {
 public:
  explicit StorageLoadedDataAndroid(JNIEnv* env,
                                    std::unique_ptr<StorageLoadedData> data);
  ~StorageLoadedDataAndroid();

  StorageLoadedDataAndroid(const StorageLoadedDataAndroid&) = delete;
  StorageLoadedDataAndroid& operator=(const StorageLoadedDataAndroid&) = delete;

  // This C++ object is owned by the Java counterpart, and should be destroyed
  // by it.
  void Destroy(JNIEnv* env);

  void OnTabRejected(JNIEnv* env, int tab_android_id);

  StorageLoadedData* GetData() const;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;

  static StorageLoadedDataAndroid* FromJavaObject(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& obj);

 private:
  std::unique_ptr<StorageLoadedData> data_;
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
};

base::android::ScopedJavaLocalRef<jobject> CreateLoadedTabState(
    JNIEnv* env,
    tabs_pb::TabState& tab_state);

}  // namespace tabs

namespace jni_zero {

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<tabs::StorageLoadedDataAndroid>(
    JNIEnv* env,
    const tabs::StorageLoadedDataAndroid& input) {
  return input.GetJavaObject();
}

template <>
inline tabs::StorageLoadedDataAndroid*
FromJniType<tabs::StorageLoadedDataAndroid*>(JNIEnv* env,
                                             const JavaRef<jobject>& obj) {
  return tabs::StorageLoadedDataAndroid::FromJavaObject(env, obj);
}

// TODO(469809169): Rather than using a const_cast, declare the function as
// tabs_pb::TabState&& and having our codegen use std::move().
template <>
inline ScopedJavaLocalRef<jobject> ToJniType<tabs_pb::TabState>(
    JNIEnv* env,
    const tabs_pb::TabState& input) {
  return tabs::CreateLoadedTabState(env, const_cast<tabs_pb::TabState&>(input));
}

}  // namespace jni_zero

#endif  // CHROME_BROWSER_ANDROID_STORAGE_LOADED_DATA_ANDROID_H_
