// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/collection_storage_observer.h"

#include "chrome/browser/tab/tab_state_storage_service.h"

namespace tabs {

CollectionStorageObserver::CollectionStorageObserver(
    tabs::TabStateStorageService* service)
    : service_(service) {}

CollectionStorageObserver::~CollectionStorageObserver() = default;

void CollectionStorageObserver::OnChildrenAdded(
    const TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles,
    bool insert_from_detached) {
  for (const auto& handle : handles) {
    if (std::holds_alternative<TabCollection::Handle>(handle)) {
      const TabCollection* collection =
          std::get<TabCollection::Handle>(handle).Get();
      service_->Save(collection);
    } else {
      const TabInterface* tab = std::get<TabHandle>(handle).Get();
      service_->Save(tab);
    }
  }
}

void CollectionStorageObserver::OnChildrenRemoved(
    const TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles) {
  for (const auto& handle : handles) {
    if (std::holds_alternative<TabCollection::Handle>(handle)) {
      const TabCollection* collection =
          std::get<TabCollection::Handle>(handle).Get();
      service_->Remove(collection, position.parent_handle.Get());
    } else {
      const TabInterface* tab = std::get<TabHandle>(handle).Get();
      service_->Remove(tab, position.parent_handle.Get());
    }
  }
}

void CollectionStorageObserver::OnChildMoved(
    const TabCollection::Position& to_position,
    const NodeData& node_data) {
  TabCollection::NodeHandle handle = node_data.handle;
  const TabCollection* prev_parent = node_data.position.parent_handle.Get();
  if (std::holds_alternative<TabCollection::Handle>(handle)) {
    const TabCollection* collection =
        std::get<TabCollection::Handle>(handle).Get();
    service_->Move(collection, prev_parent);
  } else {
    const TabInterface* tab = std::get<TabHandle>(handle).Get();
    service_->Move(tab, prev_parent);
  }
}

}  // namespace tabs
