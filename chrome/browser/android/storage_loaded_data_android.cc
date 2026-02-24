// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/storage_loaded_data_android.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/token_android.h"
#include "base/functional/bind.h"
#include "chrome/browser/android/restore_entity_tracker_android.h"
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
          tab_state.is_pinned(), tab_state.url());

  return Java_StorageLoadedData_createLoadedTabState(env, tab_state.tab_id(),
                                                     j_tab_state);
}

StorageLoadedDataAndroid::StorageLoadedDataAndroid(
    JNIEnv* env,
    std::unique_ptr<StorageLoadedData> data)
    : data_(std::move(data)) {
  std::vector<TabGroupCollectionDataAndroid*> tab_group_collection_data_android;
  for (auto& loaded_group : data_->GetLoadedGroups()) {
    auto* android_group =
        new TabGroupCollectionDataAndroid(std::move(loaded_group));
    tab_group_collection_data_android.push_back(android_group);
  }
  const StorageLoadedData::StorageLoadingContext& context =
      data_->GetLoadingContext();
  j_object_ = Java_StorageLoadedData_createData(
      env, reinterpret_cast<intptr_t>(this), data_->GetLoadedTabs(),
      tab_group_collection_data_android,
      data_->GetActiveTabIndex().value_or(-1),
      static_cast<int>(context.status()), context.error_message());
}

StorageLoadedDataAndroid::~StorageLoadedDataAndroid() = default;

void StorageLoadedDataAndroid::Destroy(JNIEnv* env) {
  delete this;
}

void StorageLoadedDataAndroid::OnTabRejected(JNIEnv* env, int tab_android_id) {
  RestoreEntityTrackerAndroid* tracker =
      static_cast<RestoreEntityTrackerAndroid*>(data_->GetTracker());
  std::optional<StorageId> node_id =
      tracker->GetStorageIdForTab(tab_android_id);
  if (node_id.has_value()) {
    GetData()->NotifyNodeRejected(*node_id);
  }
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
