// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/storage_loaded_data_android.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_string.h"
#include "base/android/token_android.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/storage_loading_context.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/StorageLoadedData_jni.h"
#include "chrome/browser/tab/jni_headers/TabGroupCollectionData_jni.h"

namespace tabs {

base::android::ScopedJavaLocalRef<jobject> CreateLoadedTabState(
    JNIEnv* env,
    tabs_pb::TabState& tab_state) {
  base::android::ScopedJavaLocalRef<jobject> j_web_contents_state_buffer;
  // Take ownership of the protobuf's C++ `std::string`, copy its bytes into a
  // Java-owned direct `ByteBuffer` (`ByteBuffer.allocateDirect`) when
  // non-empty, and free the C++ `std::string` immediately when `state_bytes`
  // goes out of scope on the DB sequence. Once stored in `WebContentsState`,
  // the `DirectByteBuffer`'s off-heap memory is owned by Java and freed
  // automatically by the Android GC when unreachable (no manual
  // `WebContentsState#destroy()` needed).
  if (tab_state.has_web_contents_state_bytes()) {
    std::unique_ptr<std::string> state_bytes(
        tab_state.release_web_contents_state_bytes());
    if (!state_bytes->empty() &&
        base::IsValueInRangeForNumericType<int>(state_bytes->size())) {
      j_web_contents_state_buffer = jni_zero::ByteBufferAllocateDirect(
          env, base::checked_cast<int>(state_bytes->size()));
      if (j_web_contents_state_buffer) {
        base::android::JavaByteBufferToMutableSpan(env,
                                                   j_web_contents_state_buffer)
            .copy_from(base::as_byte_span(*state_bytes));
      }
    }
  }

  const tabs_pb::Token& tab_group_id = tab_state.tab_group_id();
  base::Token tab_group_token(tab_group_id.high(), tab_group_id.low());
  base::android::ScopedJavaLocalRef<jobject> j_tab_group_id;
  if (!tab_group_token.is_zero()) {
    j_tab_group_id = base::android::TokenAndroid::Create(env, tab_group_token);
  }

  base::android::ScopedJavaLocalRef<jobject> j_tab_state =
      Java_StorageLoadedData_createTabState(
          env, tab_state.parent_id(), tab_state.timestamp_millis(),
          j_web_contents_state_buffer, tab_state.web_contents_state_version(),
          tab_state.opener_app_id(), tab_state.theme_color(),
          tab_state.launch_type_at_creation(), tab_state.user_agent(),
          tab_state.last_navigation_committed_timestamp_millis(),
          j_tab_group_id, tab_state.tab_has_sensitive_content(),
          tab_state.is_pinned(), tab_state.url());

  return Java_StorageLoadedData_createLoadedTabState(env, tab_state.tab_id(),
                                                     j_tab_state);
}

base::android::ScopedJavaLocalRef<jobject> CreateStorageLoadWarning(
    JNIEnv* env,
    const StorageLoadingContext::Warning& warning) {
  return Java_StorageLoadedData_createStorageLoadWarning(
      env, static_cast<int>(warning.status), warning.message);
}

base::android::ScopedJavaLocalRef<jobject> CreateTabGroupCollectionData(
    JNIEnv* env,
    const TabGroupCollectionData& data) {
  return Java_TabGroupCollectionData_create(
      env, data.tab_group_id_, data.title_, static_cast<int>(data.color_),
      data.is_collapsed_);
}

StorageLoadedDataAndroid::StorageLoadedDataAndroid(
    std::unique_ptr<StorageLoadedData> data)
    : data_(std::move(data)) {}

StorageLoadedDataAndroid::~StorageLoadedDataAndroid() = default;

// static
base::android::ScopedJavaLocalRef<jobject>
StorageLoadedDataAndroid::CreateJavaObject(
    JNIEnv* env,
    std::unique_ptr<StorageLoadedData> data) {
  CHECK(data);
  // Extract the tab and group vectors out of `data` before moving `data` into
  // `StorageLoadedDataAndroid` so the C++ `tabs_pb::TabState` and
  // `TabGroupCollectionData` payloads are freed as soon as JNI marshalling
  // finishes on the DB sequence. Java `StorageLoadedData.mNativePtr`
  // (`JniUniquePtr`) is the single owner of `StorageLoadedDataAndroid`.
  std::vector<tabs_pb::TabState> loaded_tabs =
      std::exchange(data->GetLoadedTabs(), {});
  std::vector<std::unique_ptr<TabGroupCollectionData>> loaded_groups =
      std::exchange(data->GetLoadedGroups(), {});
  int active_tab_index = data->GetActiveTabIndex().value_or(-1);
  auto native_wrapper =
      base::WrapUnique(new StorageLoadedDataAndroid(std::move(data)));
  const auto& warnings =
      native_wrapper->GetData()->GetLoadingContext().warnings();
  return Java_StorageLoadedData_createData(
      env,
      jni_zero::MakeUnique<StorageLoadedDataAndroid>(std::move(native_wrapper)),
      loaded_tabs, loaded_groups, active_tab_index, warnings);
}

// static
void StorageLoadedDataAndroid::DestroyJavaObject(
    const jni_zero::JavaRef<jobject>& j_object) {
  CHECK(j_object);
  Java_StorageLoadedData_destroy(jni_zero::AttachCurrentThread(), j_object);
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
  return Java_StorageLoadedData_getNativePtr(env, obj);
}

}  // namespace tabs

DEFINE_JNI(StorageLoadedData)
DEFINE_JNI(TabGroupCollectionData)
