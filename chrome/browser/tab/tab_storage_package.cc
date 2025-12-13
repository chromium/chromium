// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_package.h"

#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/protocol/token.pb.h"

namespace tabs {

TabStoragePackage::TabStoragePackage(int user_agent,
                                     base::Token tab_group_id,
                                     bool is_pinned,
                                     AndroidTabPackage android_tab_package)
    : user_agent_(user_agent),
      tab_group_id_(std::move(tab_group_id)),
      is_pinned_(is_pinned),
      android_tab_package_(std::move(android_tab_package)) {}

TabStoragePackage::~TabStoragePackage() = default;

std::vector<uint8_t> TabStoragePackage::SerializePayload() const {
  tabs_pb::TabState tab_state;
  tab_state.set_tab_id(android_tab_package_.id_);
  tab_state.set_parent_id(android_tab_package_.parent_id_);
  tab_state.set_timestamp_millis(android_tab_package_.timestamp_millis_);
  if (android_tab_package_.web_contents_state_bytes_) {
    tab_state.set_web_contents_state_bytes(
        android_tab_package_.web_contents_state_bytes_->data(),
        android_tab_package_.web_contents_state_bytes_->size());
  }
  tab_state.set_web_contents_state_version(android_tab_package_.version_);
  if (android_tab_package_.opener_app_id_) {
    tab_state.set_opener_app_id(*android_tab_package_.opener_app_id_);
  }
  tab_state.set_theme_color(android_tab_package_.theme_color_);
  tab_state.set_launch_type_at_creation(
      android_tab_package_.launch_type_at_creation_);
  tab_state.set_last_navigation_committed_timestamp_millis(
      android_tab_package_.last_navigation_committed_timestamp_millis_);
  tab_state.set_tab_has_sensitive_content(
      android_tab_package_.tab_has_sensitive_content_);
  tab_state.set_user_agent(user_agent_);
  tabs_pb::Token* tab_group_id = tab_state.mutable_tab_group_id();
  tab_group_id->set_high(tab_group_id_.high());
  tab_group_id->set_low(tab_group_id_.low());
  tab_state.set_is_pinned(is_pinned_);

  std::vector<uint8_t> payload_vec(tab_state.ByteSizeLong());
  tab_state.SerializeToArray(payload_vec.data(), payload_vec.size());
  return payload_vec;
}

std::vector<uint8_t> TabStoragePackage::SerializeChildren() const {
  return {};
}

}  // namespace tabs
