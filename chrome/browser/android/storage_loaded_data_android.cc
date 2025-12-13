// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/storage_loaded_data_android.h"

#include <algorithm>
#include <memory>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_callback.h"
#include "base/android/jni_string.h"
#include "base/android/token_android.h"
#include "base/functional/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_group_collection_data_android.h"
#include "chrome/browser/android/tab_state_storage_service_factory.h"
#include "chrome/browser/tab/collection_save_forwarder.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/StorageLoadedData_jni.h"

namespace tabs {

namespace {

using ScopedJavaLocalRef = base::android::ScopedJavaLocalRef<jobject>;

base::android::ScopedJavaLocalRef<jobject> CreateLoadedTabState(
    JNIEnv* env,
    tabs_pb::TabState& tab_state) {
  base::android::ScopedJavaLocalRef<jobject> j_web_contents_state_buffer;
  long j_web_contents_state_string_pointer = 0;
  if (tab_state.has_web_contents_state_bytes()) {
    std::string* web_contents_state_bytes_ptr =
        tab_state.release_web_contents_state_bytes();
    j_web_contents_state_buffer =
        base::android::ScopedJavaLocalRef<jobject>::Adopt(
            env, env->NewDirectByteBuffer(
                     static_cast<void*>(web_contents_state_bytes_ptr->data()),
                     web_contents_state_bytes_ptr->size()));
    j_web_contents_state_string_pointer =
        reinterpret_cast<long>(web_contents_state_bytes_ptr);
  }

  const tabs_pb::Token& tab_group_id = tab_state.tab_group_id();
  base::Token tab_group_token(tab_group_id.high(), tab_group_id.low());
  base::android::ScopedJavaLocalRef<jobject> j_tab_group_id;
  if (!tab_group_token.is_zero()) {
    j_tab_group_id = base::android::TokenAndroid::Create(env, tab_group_token);
  }

  base::android::ScopedJavaLocalRef<jobject> j_tab_state =
      Java_StorageLoadedData_createTabState(
          env, tab_state.parent_id(), tab_state.root_id(),
          tab_state.timestamp_millis(), j_web_contents_state_buffer,
          tab_state.web_contents_state_version(),
          j_web_contents_state_string_pointer, tab_state.opener_app_id(),
          tab_state.theme_color(), tab_state.launch_type_at_creation(),
          tab_state.user_agent(),
          tab_state.last_navigation_committed_timestamp_millis(),
          j_tab_group_id, tab_state.tab_has_sensitive_content(),
          tab_state.is_pinned());

  return Java_StorageLoadedData_createLoadedTabState(env, tab_state.tab_id(),
                                                     j_tab_state);
}

base::android::ScopedJavaLocalRef<jobjectArray> CreateLoadedTabStates(
    JNIEnv* env,
    std::vector<tabs_pb::TabState>& loaded_tabs) {
  std::vector<base::android::ScopedJavaLocalRef<jobject>>
      j_loaded_tab_state_vector;
  for (auto& loaded_tab : loaded_tabs) {
    j_loaded_tab_state_vector.push_back(CreateLoadedTabState(env, loaded_tab));
  }

  base::android::ScopedJavaLocalRef<jclass> type = base::android::GetClass(
      env, "org/chromium/chrome/browser/tab/StorageLoadedData$LoadedTabState");
  return base::android::ToTypedJavaArrayOfObjects(
      env, j_loaded_tab_state_vector, type.obj());
}

base::android::ScopedJavaLocalRef<jobjectArray> CreateGroupCollectionData(
    JNIEnv* env,
    std::vector<std::unique_ptr<TabGroupCollectionData>>& loaded_groups) {
  std::vector<base::android::ScopedJavaLocalRef<jobject>> j_loaded_group_vector;
  for (auto& loaded_group : loaded_groups) {
    auto* android_group =
        new TabGroupCollectionDataAndroid(std::move(loaded_group));
    j_loaded_group_vector.push_back(android_group->GetJavaObject());
  }

  base::android::ScopedJavaLocalRef<jclass> type = base::android::GetClass(
      env, "org/chromium/chrome/browser/tab/TabGroupCollectionData");
  return base::android::ToTypedJavaArrayOfObjects(env, j_loaded_group_vector,
                                                  type.obj());
}

}  // namespace

StorageLoadedDataAndroid::StorageLoadedDataAndroid(
    JNIEnv* env,
    std::unique_ptr<StorageLoadedData> data)
    : data_(std::move(data)) {
  base::android::ScopedJavaLocalRef<jobjectArray> loaded_tab_states =
      CreateLoadedTabStates(env, data_->GetLoadedTabs());
  base::android::ScopedJavaLocalRef<jobjectArray> loaded_groups =
      CreateGroupCollectionData(env, data_->GetLoadedGroups());
  j_object_ = Java_StorageLoadedData_createData(
      env, reinterpret_cast<intptr_t>(this), loaded_tab_states, loaded_groups,
      data_->GetActiveTabIndex().value_or(-1));
}

StorageLoadedDataAndroid::~StorageLoadedDataAndroid() = default;

void StorageLoadedDataAndroid::Destroy(JNIEnv* env) {
  delete this;
}

base::android::ScopedJavaLocalRef<jobject>
StorageLoadedDataAndroid::GetJavaObject() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return j_object_.AsLocalRef(env);
}

StorageLoadedData* StorageLoadedDataAndroid::GetData() const {
  return data_.get();
}

// static
StorageLoadedDataAndroid* StorageLoadedDataAndroid::FromJavaObject(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj) {
  if (!obj) {
    return nullptr;
  }
  return reinterpret_cast<StorageLoadedDataAndroid*>(
      Java_StorageLoadedData_getNativePtr(env, obj));
}

}  // namespace tabs

DEFINE_JNI(StorageLoadedData)
