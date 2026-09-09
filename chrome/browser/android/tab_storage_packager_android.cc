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
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/token.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_group_android.h"
#include "chrome/browser/android/tab_group_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/protocol/tab_group_collection_state.pb.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/protocol/tab_strip_collection_state.pb.h"
#include "chrome/browser/tab/protocol/token.pb.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "chrome/browser/tab/web_contents_state.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabStoragePackager_jni.h"

namespace tabs {

struct TabStorageMetadata {
  int64_t timestamp_millis = 0;
  int32_t theme_color = 0;
  int64_t last_navigation_committed_timestamp_millis = 0;
  bool tab_has_sensitive_content = false;
  std::optional<std::string> opener_app_id;
};

static void JNI_TabStoragePackager_OnTabStorageMetadataFetched(
    JNIEnv* env,
    int64_t metadata_ptr,
    int64_t timestamp_millis,
    int32_t theme_color,
    int64_t last_navigation_committed_timestamp_millis,
    bool tab_has_sensitive_content,
    std::optional<std::string> opener_app_id) {
  auto* metadata = reinterpret_cast<TabStorageMetadata*>(metadata_ptr);
  metadata->timestamp_millis = timestamp_millis;
  metadata->theme_color = theme_color;
  metadata->last_navigation_committed_timestamp_millis =
      last_navigation_committed_timestamp_millis;
  metadata->tab_has_sensitive_content = tab_has_sensitive_content;
  metadata->opener_app_id = std::move(opener_app_id);
}

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(tab);
  const TabAndroid* tab_android = TabAndroid::FromTabInterface(tab);
  CHECK(tab_android);

  tabs_pb::TabState tab_state;
  tab_state.set_tab_id(tab_android->GetAndroidId());
  tab_state.set_parent_id(tab_android->GetParentId());
  tab_state.set_launch_type_at_creation(
      tab_android->GetTabLaunchTypeAtCreation());
  tab_state.set_user_agent(tab_android->GetUserAgent());
  tab_state.set_is_pinned(tab_android->IsPinned());

  base::Token tab_group_id;
  if (tab_android->GetGroup().has_value()) {
    tab_group_id = tab_android->GetGroup()->token();
  }
  tabs_pb::Token* proto_tab_group_id = tab_state.mutable_tab_group_id();
  proto_tab_group_id->set_high(tab_group_id.high());
  proto_tab_group_id->set_low(tab_group_id.low());

  GURL gurl = tab_android->GetURL();
  if (gurl.is_valid()) {
    tab_state.set_url(gurl.spec());
  }

  if (content::WebContents* web_contents = tab_android->web_contents()) {
    if (WebContentsState::WriteContentsState(
            web_contents, tab_state.mutable_web_contents_state_bytes())) {
      tab_state.set_web_contents_state_version(2);
    } else {
      tab_state.clear_web_contents_state_bytes();
      tab_state.set_web_contents_state_version(-1);
    }
  } else {
    std::unique_ptr<WebContentsStateByteBuffer> byte_buffer =
        tab_android->GetWebContentsByteBuffer();
    if (byte_buffer && !byte_buffer->GetBuffer().empty()) {
      base::span<const uint8_t> buffer_span = byte_buffer->GetBuffer();
      tab_state.set_web_contents_state_bytes(
          reinterpret_cast<const char*>(buffer_span.data()),
          buffer_span.size());
      tab_state.set_web_contents_state_version(byte_buffer->state_version());
    } else {
      tab_state.set_web_contents_state_version(-1);
    }
  }

  TabStorageMetadata metadata;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabStoragePackager_fetchTabStorageMetadata(
      env, tab_android, reinterpret_cast<intptr_t>(&metadata));

  tab_state.set_timestamp_millis(metadata.timestamp_millis);
  tab_state.set_theme_color(metadata.theme_color);
  tab_state.set_last_navigation_committed_timestamp_millis(
      metadata.last_navigation_committed_timestamp_millis);
  tab_state.set_tab_has_sensitive_content(metadata.tab_has_sensitive_content);
  if (metadata.opener_app_id.has_value()) {
    tab_state.set_opener_app_id(*metadata.opener_app_id);
  }

  return std::make_unique<TabStoragePackage>(std::move(tab_state));
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

int64_t TabStoragePackagerAndroid::ConsolidateTabStripCollectionData(
    JNIEnv* env,
    std::string window_tag,
    int32_t j_tab_model_type,
    TabAndroid* active_tab) {
  tabs_pb::TabStripCollectionState state;

  state.set_window_tag(std::move(window_tag));
  state.set_tab_model_type(j_tab_model_type);

  UnmappedTabStripCollectionStorageData* data =
      new UnmappedTabStripCollectionStorageData(active_tab, std::move(state));
  return reinterpret_cast<intptr_t>(data);
}

TabStoragePackagerAndroid::~TabStoragePackagerAndroid() = default;

}  // namespace tabs

DEFINE_JNI(TabStoragePackager)
