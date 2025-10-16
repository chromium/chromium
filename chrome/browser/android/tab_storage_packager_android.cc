// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_storage_packager_android.h"

#include <jni.h>

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/token.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_group_android.h"
#include "chrome/browser/android/tab_group_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/android_tab_package.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/protocol/tab_group_collection_state.pb.h"
#include "chrome/browser/tab/protocol/tab_strip_collection_state.pb.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "components/tabs/public/direct_child_walker.h"
#include "components/tabs/public/tab_strip_collection.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabStoragePackager_jni.h"

namespace tabs {
static const int kTabStoragePackagerAndroidVersion = 1;

// A payload of data representing TabGroupTabCollection.
class TabGroupCollectionStorageData : public Payload {
 public:
  explicit TabGroupCollectionStorageData(tabs_pb::TabGroupCollectionState state)
      : state_(std::move(state)) {}

  ~TabGroupCollectionStorageData() override = default;

  std ::string SerializePayload() const override {
    return state_.SerializeAsString();
  }

 private:
  tabs_pb::TabGroupCollectionState state_;
};

// A payload of data representing TabStripCollection.
class TabStripCollectionStorageData : public Payload {
 public:
  explicit TabStripCollectionStorageData(tabs_pb::TabStripCollectionState state)
      : state_(std::move(state)) {}

  ~TabStripCollectionStorageData() override = default;

  std ::string SerializePayload() const override {
    return state_.SerializeAsString();
  }

 private:
  tabs_pb::TabStripCollectionState state_;
};

TabStoragePackagerAndroid::TabStoragePackagerAndroid(Profile* profile)
    : profile_(profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      Java_TabStoragePackager_create(env, reinterpret_cast<intptr_t>(this)));
}

std::unique_ptr<StoragePackage> TabStoragePackagerAndroid::Package(
    const TabInterface* tab) {
  JNIEnv* env = base::android::AttachCurrentThread();
  long ptr_value = Java_TabStoragePackager_packageTab(
      env, java_obj_, static_cast<const TabAndroid*>(tab));
  TabStoragePackage* data = reinterpret_cast<TabStoragePackage*>(ptr_value);

  return base::WrapUnique(data);
}

std::unique_ptr<Payload>
TabStoragePackagerAndroid::PackageTabStripCollectionData(
    const TabStripCollection* collection,
    StorageIdMapping& mapping) {
  JNIEnv* env = base::android::AttachCurrentThread();
  long ptr_value = Java_TabStoragePackager_packageTabStripCollection(
      env, java_obj_, profile_, collection);
  TabStripCollectionStorageData* data =
      reinterpret_cast<TabStripCollectionStorageData*>(ptr_value);

  return base::WrapUnique(data);
}

std::unique_ptr<Payload>
TabStoragePackagerAndroid::PackageTabGroupTabCollectionData(
    const TabGroupTabCollection* collection,
    StorageIdMapping& mapping) {
  tabs_pb::TabGroupCollectionState state;
  const base::Token group_id = collection->GetTabGroupId().token();
  state.set_group_id_high(group_id.high());
  state.set_group_id_low(group_id.low());

  const tab_groups::TabGroupVisualData* visual_data =
      collection->GetTabGroup()->visual_data();
  state.set_color(static_cast<int32_t>(visual_data->color()));
  state.set_is_collapsed(visual_data->is_collapsed());
  state.set_title(base::UTF16ToUTF8(visual_data->title()));

  return std::make_unique<TabGroupCollectionStorageData>(std::move(state));
}

long TabStoragePackagerAndroid::ConsolidateTabData(
    JNIEnv* env,
    jlong timestamp_millis,
    const jni_zero::JavaParamRef<jobject>& web_contents_state_buffer,
    std::string& opener_app_id,
    jint theme_color,
    jlong last_navigation_committed_timestamp_millis,
    jboolean tab_has_sensitive_content,
    TabAndroid* tab) {
  std::unique_ptr<std::string> web_contents_state_bytes;
  if (web_contents_state_buffer) {
    base::span<const uint8_t> span =
        base::android::JavaByteBufferToSpan(env, web_contents_state_buffer);
    web_contents_state_bytes =
        std::make_unique<std::string>(span.begin(), span.end());
  }

  base::Token tab_group_id;
  if (tab->GetGroup().has_value()) {
    tab_group_id = tab->GetGroup()->token();
  }

  std::unique_ptr<AndroidTabPackage> android_package =
      std::make_unique<AndroidTabPackage>(
          kTabStoragePackagerAndroidVersion, tab->GetAndroidId(),
          tab->GetParentId(), timestamp_millis,
          std::move(web_contents_state_bytes),
          std::make_unique<std::string>(std::move(opener_app_id)), theme_color,
          last_navigation_committed_timestamp_millis, tab_has_sensitive_content,
          tab->GetTabLaunchTypeAtCreation());

  TabStoragePackage* package_ptr =
      new TabStoragePackage(tab->GetUserAgent(), std::move(tab_group_id),
                            tab->IsPinned(), std::move(android_package));

  return reinterpret_cast<long>(package_ptr);
}

long TabStoragePackagerAndroid::ConsolidateTabStripCollectionData(
    JNIEnv* env,
    jint window_id,
    jint j_tab_model_type) {
  tabs_pb::TabStripCollectionState state;

  state.set_window_id(window_id);
  state.set_tab_model_type(j_tab_model_type);

  TabStripCollectionStorageData* data =
      new TabStripCollectionStorageData(state);
  return reinterpret_cast<long>(data);
}

TabStoragePackagerAndroid::~TabStoragePackagerAndroid() = default;

}  // namespace tabs
