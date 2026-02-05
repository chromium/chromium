// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/collection_storage_observer.h"

#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/tab_collection.h"

namespace tabs {

CollectionStorageObserver::CollectionStorageObserver(
    tabs::TabStateStorageService* service)
    : service_(service) {}

CollectionStorageObserver::~CollectionStorageObserver() = default;

void CollectionStorageObserver::SaveChildNodeOnly(
    TabCollectionNodeHandle handle) {
  if (std::holds_alternative<TabCollection::Handle>(handle)) {
    const TabCollection* collection =
        std::get<TabCollection::Handle>(handle).Get();
    if (collection->GetParentCollection()) {
      service_->Save(collection);
    }
  } else {
    const TabInterface* tab = std::get<TabHandle>(handle).Get();
    if (tab->GetParentCollection()) {
      service_->Save(tab);
    }
  }
}

void CollectionStorageObserver::OnChildrenAdded(
    const TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles,
    bool insert_from_detached) {
  for (const auto& handle : handles) {
    const TabCollection* parent;
    if (std::holds_alternative<TabCollection::Handle>(handle)) {
      const TabCollection* collection =
          std::get<TabCollection::Handle>(handle).Get();
      service_->Save(collection);
      parent = collection->GetParentCollection();
    } else {
      const TabInterface* tab = std::get<TabHandle>(handle).Get();
      service_->Save(tab);
      parent = tab->GetParentCollection();
    }

    DCHECK(parent) << "Child node should have parent";
    service_->SaveChildren(parent);
  }
}

void CollectionStorageObserver::OnChildrenRemoved(
    const TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles) {
  for (const auto& handle : handles) {
    if (std::holds_alternative<TabCollection::Handle>(handle)) {
      const TabCollection* collection =
          std::get<TabCollection::Handle>(handle).Get();
      service_->Remove(collection);
    } else {
      const TabInterface* tab = std::get<TabHandle>(handle).Get();
      service_->Remove(tab);
    }
    service_->SaveChildren(position.parent_handle.Get());
  }
}

void CollectionStorageObserver::OnChildMoved(
    const TabCollection::Position& to_position,
    const NodeData& node_data) {
  TabCollectionNodeHandle handle = node_data.handle;
  const TabCollection* curr_parent;
  const TabCollection* prev_parent = node_data.position.parent_handle.Get();
  if (std::holds_alternative<TabCollectionHandle>(handle)) {
    const TabCollection* collection =
        std::get<TabCollection::Handle>(handle).Get();
    curr_parent = collection->GetParentCollection();
  } else {
    const TabInterface* tab = std::get<TabHandle>(handle).Get();
    curr_parent = tab->GetParentCollection();
  }

  DCHECK(curr_parent) << "Child node should have parent";
  service_->SaveChildren(curr_parent);
  if (curr_parent != prev_parent) {
    service_->SaveChildren(prev_parent);
  }
}

}  // namespace tabs
