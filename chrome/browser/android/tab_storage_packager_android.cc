// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_storage_packager_android.h"

#include <jni.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/token.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_android_conversions.h"
#include "chrome/browser/android/tab_group_android.h"
#include "chrome/browser/android/tab_group_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/android_tab_package.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/protocol/tab_group_collection_state.pb.h"
#include "chrome/browser/tab/protocol/tab_strip_collection_state.pb.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "components/tabs/public/tab_strip_collection.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabStoragePackager_jni.h"

namespace tabs {
// TODO(crbug.com/430996004): Reference a shared constant for the web content
// state.
static const int kTabStoragePackagerAndroidVersion = 2;

// A payload of data representing TabStripCollection.
class TabStripCollectionStorageData : public Payload {
 public:
  explicit TabStripCollectionStorageData(tabs_pb::TabStripCollectionState state)
      : state_(std::move(state)) {}

  ~TabStripCollectionStorageData() override = default;

  std::vector<uint8_t> SerializePayload() const override {
    std::vector<uint8_t> payload_vec(state_.ByteSizeLong());
    state_.SerializeToArray(payload_vec.data(), payload_vec.size());
    return payload_vec;
  }

 private:
  tabs_pb::TabStripCollectionState state_;
};

// A wrapper around TabStripCollectionState that has not had a StorageIdMapping
// applied to it so the data is still unmapped (i.e. we have references to
// objects that need to be converted to storage ids).
class UnmappedTabStripCollectionStorageData {
 public:
  UnmappedTabStripCollectionStorageData(TabAndroid* active_tab,
                                        tabs_pb::TabStripCollectionState state)
      : active_tab_(active_tab), state_(std::move(state)) {}

  ~UnmappedTabStripCollectionStorageData() = default;

  TabAndroid* active_tab() const { return active_tab_.get(); }

  // Moves the state out of this object. This should only be called once.
  tabs_pb::TabStripCollectionState TakeState() {
    CHECK(is_valid_) << "Attempting to take state multiple times.";
    is_valid_ = false;
    return std::move(state_);
  }

 private:
  bool is_valid_{true};
  // May be nullptr if there is no active tab in the collection (i.e. there are
  // no tabs in the tab strip).
  raw_ptr<TabAndroid> active_tab_;
  tabs_pb::TabStripCollectionState state_;
};

// Consumes `unmapped_data` and applies the `mapping` to it. The returned
// TabStripCollectionStorageData is a valid payload that can be packaged into
// the database.
std::unique_ptr<TabStripCollectionStorageData>
MapAndConsumeUnmappedTabStripCollectionStorageData(
    std::unique_ptr<UnmappedTabStripCollectionStorageData> unmapped_data,
    StorageIdMapping& mapping) {
  tabs_pb::TabStripCollectionState state = unmapped_data->TakeState();
  TabAndroid* active_tab = unmapped_data->active_tab();
  if (active_tab) {
    tabs_pb::Token* active_tab_storage_id =
        state.mutable_active_tab_storage_id();
    StorageIdToTokenProto(mapping.GetStorageId(active_tab),
                          active_tab_storage_id);
  }
  return std::make_unique<TabStripCollectionStorageData>(std::move(state));
}

TabStoragePackagerAndroid::TabStoragePackagerAndroid(Profile* profile)
    : profile_(profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      Java_TabStoragePackager_create(env, reinterpret_cast<intptr_t>(this)));
}

bool TabStoragePackagerAndroid::IsOffTheRecord(
    const TabCollection* collection) const {
  const TabCollection* root_collection = GetRootCollection(collection);

  JNIEnv* env = base::android::AttachCurrentThread();
  return static_cast<bool>(Java_TabStoragePackager_isOffTheRecord(
      env, java_obj_, profile_,
      static_cast<const TabStripCollection*>(root_collection)));
}

std::string TabStoragePackagerAndroid::GetWindowTag(
    const TabCollection* collection) const {
  const TabCollection* root_collection = GetRootCollection(collection);

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TabStoragePackager_getWindowTag(
      env, java_obj_, profile_,
      static_cast<const TabStripCollection*>(root_collection));
}

std::unique_ptr<StoragePackage> TabStoragePackagerAndroid::Package(
    const TabInterface* tab) {
  JNIEnv* env = base::android::AttachCurrentThread();
  long ptr_value = Java_TabStoragePackager_packageTab(env, java_obj_,
                                                      ToTabAndroidChecked(tab));
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
  return MapAndConsumeUnmappedTabStripCollectionStorageData(
      base::WrapUnique(
          reinterpret_cast<UnmappedTabStripCollectionStorageData*>(ptr_value)),
      mapping);
}

long TabStoragePackagerAndroid::ConsolidateTabData(
    JNIEnv* env,
    jlong timestamp_millis,
    const jni_zero::JavaParamRef<jobject>& web_contents_state_buffer,
    std::optional<std::string> opener_app_id,
    jint theme_color,
    jlong last_navigation_committed_timestamp_millis,
    jboolean tab_has_sensitive_content,
    TabAndroid* tab) {
  std::optional<std::vector<uint8_t>> web_contents_state_bytes;
  if (web_contents_state_buffer) {
    base::span<const uint8_t> span =
        base::android::JavaByteBufferToSpan(env, web_contents_state_buffer);
    web_contents_state_bytes.emplace(span.begin(), span.end());
  }

  base::Token tab_group_id;
  if (tab->GetGroup().has_value()) {
    tab_group_id = tab->GetGroup()->token();
  }

  AndroidTabPackage android_package(
      kTabStoragePackagerAndroidVersion, tab->GetAndroidId(),
      tab->GetParentId(), timestamp_millis, std::move(web_contents_state_bytes),
      std::move(opener_app_id), theme_color,
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
    jint j_tab_model_type,
    TabAndroid* active_tab) {
  tabs_pb::TabStripCollectionState state;

  state.set_window_id(window_id);
  state.set_tab_model_type(j_tab_model_type);

  UnmappedTabStripCollectionStorageData* data =
      new UnmappedTabStripCollectionStorageData(active_tab, std::move(state));
  return reinterpret_cast<long>(data);
}

TabStoragePackagerAndroid::~TabStoragePackagerAndroid() = default;

}  // namespace tabs

DEFINE_JNI(TabStoragePackager)
