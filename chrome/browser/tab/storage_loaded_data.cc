// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_loaded_data.h"

namespace tabs {

StorageLoadedData::StorageLoadedData(
    std::vector<LoadedTabState> loaded_tabs,
    std::vector<std::unique_ptr<TabGroupCollectionData>> loaded_groups,
    std::unique_ptr<RestoreIdAssociator> node_associator)
    : loaded_tabs_(std::move(loaded_tabs)),
      loaded_groups_(std::move(loaded_groups)),
      node_associator_(std::move(node_associator)) {}
StorageLoadedData::~StorageLoadedData() = default;

StorageLoadedData::StorageLoadedData(StorageLoadedData&&) = default;
StorageLoadedData& StorageLoadedData::operator=(StorageLoadedData&&) = default;

RestoreIdAssociator* StorageLoadedData::GetNodeAssociator() const {
  return node_associator_.get();
}

std::vector<LoadedTabState>& StorageLoadedData::GetLoadedTabs() {
  return loaded_tabs_;
}

std::vector<std::unique_ptr<TabGroupCollectionData>>&
StorageLoadedData::GetLoadedGroups() {
  return loaded_groups_;
}

}  // namespace tabs
