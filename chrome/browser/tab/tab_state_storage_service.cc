// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_service.h"

#include "base/token.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "components/tabs/public/direct_child_walker.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

class TabStateStorageService::SaveProcessor
    : public DirectChildWalker::Processor {
 public:
  SaveProcessor(TabStateStorageService* service,
                tabs_pb::Children* children_proto)
      : service_(service), children_proto_(children_proto) {}

  void ProcessTab(const TabInterface* tab) override {
    children_proto_->add_storage_id(service_->GetOrCreateStorageId(
        tab, service_->tab_handle_to_storage_id_));
  }

  void ProcessCollection(const TabCollection* collection) override {
    children_proto_->add_storage_id(service_->GetOrCreateStorageId(
        collection, service_->collection_handle_to_storage_id_));
  }

 private:
  raw_ptr<TabStateStorageService> service_;
  raw_ptr<tabs_pb::Children> children_proto_;
};

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
    int web_content_state_version,
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
  tab_state.set_web_contents_state_version(web_content_state_version);

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
  std::string payload;
  tab_state.SerializeToString(&payload);
  // TODO(https://crbug.com/427254267): Using Java Tab id for storage id is now
  // a completely invalid approach and will conflict with storage ids generated
  // for the collections objects. This method needs to be reworked to take a
  // collections object instead.
  // TODO(https://crbug.com/427254267): Create enum for types.
  tab_backend_->SaveNode(id, 1, std::move(payload), "");
}

void TabStateStorageService::LoadAllTabs(LoadAllTabsCallback callback) {
  tab_backend_->LoadAllNodes(
      base::BindOnce(&TabStateStorageService::OnAllTabsLoaded,
                     base::Unretained(this), std::move(callback)));
}

void TabStateStorageService::OnAllTabsLoaded(LoadAllTabsCallback callback,
                                             std::vector<NodeState> entries) {
  int max_id = 0;
  std::vector<tabs_pb::TabState> tab_states;
  for (auto& entry : entries) {
    if (entry.id > max_id) {
      max_id = entry.id;
    }
    // TODO(https://crbug.com/427254267): Create enum for types.
    if (entry.type == 1) {
      tabs_pb::TabState tab_state;
      if (tab_state.ParseFromString(entry.payload)) {
        tab_states.emplace_back(std::move(tab_state));
      }
    }
  }
  // TODO(https://crbug.com/427254267): While this avoids collisions and does
  // set up a mapping for live Tab objects from Java, doesn't interact nicely
  // with collections objects mappings. Ideally this will be fully replaced as
  // soon as we can create and pass up collections objects on start up.
  next_storage_id_ = max_id + 1;
  std::move(callback).Run(std::move(tab_states));
}

void TabStateStorageService::Save(const tabs::TabCollection* tab_collection) {
  tabs_pb::Children children_proto;
  SaveProcessor processor(this, &children_proto);
  DirectChildWalker walker(tab_collection, &processor);
  walker.Walk();

  int storage_id =
      GetOrCreateStorageId(tab_collection, collection_handle_to_storage_id_);
  std::string children_string;
  children_proto.SerializeToString(&children_string);
  // TODO(https://crbug.com/427254267): Write payload for metadata about
  // collections object once extractor mechaism exists.
  // TODO(https://crbug.com/427254267): Create enum for types.
  tab_backend_->SaveNode(storage_id, 2, "", std::move(children_string));
}

template <typename T>
int TabStateStorageService::GetOrCreateStorageId(
    T* object,
    std::map<int32_t, int>& handle_map) {
  int32_t handle_id = object->GetHandle().raw_value();
  auto [it, inserted] = handle_map.try_emplace(handle_id, next_storage_id_);
  if (inserted) {
    next_storage_id_++;
  }
  return it->second;
}

}  // namespace tabs
