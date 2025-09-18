// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_service.h"

#include "base/token.h"

namespace tabs {

TabStateStorageService::TabStateStorageService(
    std::unique_ptr<TabStateStorageBackend> tab_backend)
    : tab_backend_(std::move(tab_backend)) {
  tab_backend_->Initialize();
}

TabStateStorageService::~TabStateStorageService() = default;

void TabStateStorageService::SaveTab(
    int id,
    int parent_tab_id,
    int root_id,
    long timestamp_millis,
    const std::string* web_content_state_string,
    std::string_view opener_app_id,
    int theme_color,
    int launch_type_at_creation,
    int user_agent,
    long last_navigation_committed_timestamp_millis,
    const base::Token* tab_group_id,
    bool tab_has_sensitive_content,
    bool is_pinned) {
  tabs_pb::TabState tab_state;
  tab_state.set_parent_id(parent_tab_id);
  tab_state.set_root_id(root_id);
  tab_state.set_timestamp_millis(timestamp_millis);

  if (web_content_state_string) {
    tab_state.set_web_contents_state_bytes(*web_content_state_string);
  }

  tab_state.set_opener_app_id(opener_app_id);
  tab_state.set_theme_color(theme_color);
  tab_state.set_launch_type_at_creation(launch_type_at_creation);
  tab_state.set_user_agent(user_agent);
  tab_state.set_last_navigation_committed_timestamp_millis(
      last_navigation_committed_timestamp_millis);

  if (tab_group_id) {
    tab_state.set_tab_group_id_high(tab_group_id->high());
    tab_state.set_tab_group_id_low(tab_group_id->low());
  }

  tab_state.set_tab_has_sensitive_content(tab_has_sensitive_content);
  tab_state.set_is_pinned(is_pinned);
  tab_backend_->SaveTabState(id, tab_state);
}

void TabStateStorageService::LoadAllTabs(LoadAllTabsCallback callback) {
  tab_backend_->LoadAllTabStates(std::move(callback));
}

}  // namespace tabs
