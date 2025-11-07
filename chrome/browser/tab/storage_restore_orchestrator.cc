// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_restore_orchestrator.h"

namespace tabs {

namespace {

void OnAddChildTab(
    absl::flat_hash_set<TabCollectionNodeHandle>& restored_nodes_,
    const TabCollection::NodeHandle& handle,
    TabStateStorageService* service,
    StorageLoadedData* loaded_data) {
  if (!std::holds_alternative<TabHandle>(handle)) {
    return;
  }

  TabHandle tab_handle = std::get<TabHandle>(handle);
  bool was_tab_on_disk =
      loaded_data->GetNodeAssociator()->AssociateTabAndAncestors(
          tab_handle.Get());
  if (!was_tab_on_disk || restored_nodes_.contains(handle)) {
    service->Save(tab_handle.Get());
  } else if (was_tab_on_disk) {
    restored_nodes_.insert(handle);
  }
}

void OnAddChildCollection(
    absl::flat_hash_set<TabCollectionNodeHandle>& restored_nodes_,
    const TabCollection::NodeHandle& handle,
    TabStateStorageService* service,
    StorageLoadedData* loaded_data) {
  if (!std::holds_alternative<TabCollection::Handle>(handle)) {
    return;
  }

  TabCollection::Handle collection_handle =
      std::get<TabCollection::Handle>(handle);
  bool was_collection_on_disk =
      loaded_data->GetNodeAssociator()->HasCollectionBeenAssociated(
          collection_handle);
  if (!was_collection_on_disk || restored_nodes_.contains(handle)) {
    service->Save(collection_handle.Get());
  } else if (was_collection_on_disk) {
    restored_nodes_.insert(handle);
  }
}

}  // namespace

StorageRestoreOrchestrator::StorageRestoreOrchestrator(
    TabStripCollection* collection,
    TabStateStorageService* service,
    StorageLoadedData* loaded_data)
    : collection_(collection), service_(service), loaded_data_(loaded_data) {}

StorageRestoreOrchestrator::~StorageRestoreOrchestrator() = default;

void StorageRestoreOrchestrator::OnChildrenAdded(
    const TabCollection::Position& position,
    const TabCollectionNodes& handles) {
  // Associating a tab also associates its ancestors.
  for (const auto& handle : handles) {
    OnAddChildTab(restored_nodes_, handle, service_, loaded_data_);
  }

  // Any collection not already associated by the tab-filtered pass is new and
  // must be saved.
  for (const auto& handle : handles) {
    OnAddChildCollection(restored_nodes_, handle, service_, loaded_data_);
  }
}

void StorageRestoreOrchestrator::OnChildrenRemoved(
    const TabCollectionNodes& handles) {}

void StorageRestoreOrchestrator::OnChildMoved(
    const TabCollection::Position& to_position,
    const NodeData& node_data) {}

}  // namespace tabs
