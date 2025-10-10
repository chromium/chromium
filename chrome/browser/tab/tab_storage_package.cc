// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_package.h"

#include "chrome/browser/tab/protocol/tab_state.pb.h"

namespace tabs {

TabStoragePackage::TabStoragePackage(
    int user_agent,
    base::Token tab_group_id,
    bool is_pinned,
    std::unique_ptr<AndroidTabPackage> android_tab_package)
    : user_agent_(user_agent),
      tab_group_id_(std::move(tab_group_id)),
      is_pinned_(is_pinned),
      android_tab_package_(std::move(android_tab_package)) {}

TabStoragePackage::~TabStoragePackage() = default;

std::string TabStoragePackage::SerializePayload() const {
  tabs_pb::TabState tab_state;
  const std::unique_ptr<AndroidTabPackage>& android_package =
      android_tab_package_;
  if (android_package) {
    tab_state.set_tab_id(android_package->id_);
    tab_state.set_parent_id(android_package->parent_id_);
    tab_state.set_timestamp_millis(android_package->timestamp_millis_);
    if (android_package->web_contents_state_bytes_) {
      tab_state.set_web_contents_state_bytes(
          *android_package->web_contents_state_bytes_);
    }
    tab_state.set_web_contents_state_version(android_package->version_);
    if (android_package->opener_app_id_) {
      tab_state.set_opener_app_id(*android_package->opener_app_id_);
    }
    tab_state.set_theme_color(android_package->theme_color_);
    tab_state.set_launch_type_at_creation(
        android_package->launch_type_at_creation_);
    tab_state.set_last_navigation_committed_timestamp_millis(
        android_package->last_navigation_committed_timestamp_millis_);
    tab_state.set_tab_has_sensitive_content(
        android_package->tab_has_sensitive_content_);
  }
  tab_state.set_user_agent(user_agent_);
  tab_state.set_tab_group_id_high(tab_group_id_.high());
  tab_state.set_tab_group_id_low(tab_group_id_.low());
  tab_state.set_is_pinned(is_pinned_);

  std::string payload;
  tab_state.SerializeToString(&payload);
  return payload;
}

std::string TabStoragePackage::SerializeChildren() const {
  return "";
}

}  // namespace tabs
