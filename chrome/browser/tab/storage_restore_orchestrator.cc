// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_restore_orchestrator.h"

#include <cstddef>
#include <memory>

#include "base/check.h"
#include "chrome/browser/tab/collection_storage_observer.h"
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/tab_storage_util.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

// ObserverImpl implementation.
StorageRestoreOrchestrator::ObserverImpl::ObserverImpl(
    StorageRestoreOrchestrator* orchestrator)
    : orchestrator_(orchestrator) {}

StorageRestoreOrchestrator::ObserverImpl::~ObserverImpl() = default;

void StorageRestoreOrchestrator::ObserverImpl::OnChildRejected(
    StorageId parent) {
  orchestrator_->OnChildRejected(parent);
}

void StorageRestoreOrchestrator::ObserverImpl::OnDestroyed() {
  orchestrator_->OnDataDestroyed();
}

// StorageRestoreOrchestrator implementation.
StorageRestoreOrchestrator::StorageRestoreOrchestrator(
    TabStripCollection* collection,
    TabStateStorageService* service,
    StorageLoadedData* loaded_data)
    : default_observer_(service),
      data_observer_(this),
      collection_(collection),
      service_(service),
      loaded_data_(loaded_data),
      is_data_observer_registered_(true) {
  loaded_data_->RegisterObserver(&data_observer_);
}

StorageRestoreOrchestrator::~StorageRestoreOrchestrator() {
  OnDataDestroyed();
}

void StorageRestoreOrchestrator::OnAddChildTab(
    const TabCollection::NodeHandle& handle) {
  if (!std::holds_alternative<TabHandle>(handle)) {
    return;
  }

  TabHandle tab_handle = std::get<TabHandle>(handle);
  RestoreEntityTracker* tracker = loaded_data_->GetTracker();
  bool was_tab_on_disk = tracker->AssociateTabAndAncestors(tab_handle.Get());

  const TabCollection* parent = tab_handle.Get()->GetParentCollection();
  DCHECK(parent);
  TabCollectionHandle parent_handle = parent->GetHandle();
  DCHECK(tracker->HasCollectionBeenAssociated(parent_handle));
  StorageId parent_id = service_->GetStorageId(parent);

  if (!was_tab_on_disk || restored_nodes_.contains(handle)) {
    service_->Save(tab_handle.Get());
    MaybeAddModifiedParent(parent_id, parent_handle);
  } else if (was_tab_on_disk) {
    restored_nodes_.insert(handle);
  }

  if (modified_parents_.contains(parent_id)) {
    service_->SaveChildren(parent);
  }
}

void StorageRestoreOrchestrator::OnAddChildCollection(
    const TabCollection::NodeHandle& handle) {
  if (!std::holds_alternative<TabCollection::Handle>(handle)) {
    return;
  }

  TabCollection::Handle collection_handle =
      std::get<TabCollection::Handle>(handle);
  const TabCollection* collection = collection_handle.Get();
  TabStorageType type = TabCollectionTypeToTabStorageType(collection->type());
  if (type == TabStorageType::kPinned) {
    loaded_data_->GetTracker()->AssociatePinnedCollection(
        static_cast<const PinnedTabCollection*>(collection));
  }

  const TabCollection* parent = collection_handle.Get()->GetParentCollection();
  DCHECK(parent);
  TabCollectionHandle parent_handle = parent->GetHandle();
  StorageId parent_id = service_->GetStorageId(parent);

  bool was_collection_on_disk =
      loaded_data_->GetTracker()->HasCollectionBeenAssociated(
          collection_handle);
  if (was_collection_on_disk && !restored_nodes_.contains(handle)) {
    restored_nodes_.insert(handle);
  } else {
    service_->Save(collection_handle.Get());

    if (!was_collection_on_disk) {
      MaybeAddModifiedParent(parent_id, parent_handle);
    }
  }

  if (modified_parents_.contains(parent_id)) {
    service_->SaveChildren(parent);
  }
}

void StorageRestoreOrchestrator::OnChildRejected(const StorageId parent) {
  auto it = modified_parents_.find(parent);
  if (it != modified_parents_.end()) {
    std::optional<TabCollectionHandle> handle = it->second;
    if (handle.has_value() && handle->Get()) {
      service_->SaveChildren(handle->Get());
    }
  } else {
    modified_parents_.try_emplace(parent, std::nullopt);
  }
}

void StorageRestoreOrchestrator::MaybeAddModifiedParent(
    const StorageId& id,
    std::optional<TabCollectionHandle> handle) {
  auto [it, inserted] = modified_parents_.try_emplace(id, handle);
  if (!inserted && !it->second.has_value() && handle.has_value()) {
    it->second = handle;
  }
}

void StorageRestoreOrchestrator::OnDataDestroyed() {
  if (is_data_observer_registered_) {
    DCHECK(loaded_data_);
    loaded_data_->UnregisterObserver(&data_observer_);
    is_data_observer_registered_ = false;
  }
}

void StorageRestoreOrchestrator::OnChildrenAdded(
    const TabCollection::Position& position,
    const TabCollectionNodes& handles,
    bool insert_from_detached) {
  // Associating a tab also associates its ancestors.
  for (const auto& handle : handles) {
    OnAddChildTab(handle);
  }

  // Any collection not already associated by the tab-filtered pass is new and
  // must be saved.
  for (const auto& handle : handles) {
    OnAddChildCollection(handle);
  }
}

void StorageRestoreOrchestrator::OnChildrenRemoved(
    const TabCollection::Position& position,
    const TabCollectionNodes& handles) {
  StorageId parent_id = service_->GetStorageId(position.parent_handle.Get());
  MaybeAddModifiedParent(parent_id, position.parent_handle);
  default_observer_.OnChildrenRemoved(position, handles);
}

void StorageRestoreOrchestrator::OnChildMoved(
    const TabCollection::Position& to_position,
    const NodeData& node_data) {
  StorageId to_parent_id =
      service_->GetStorageId(to_position.parent_handle.Get());
  MaybeAddModifiedParent(to_parent_id, to_position.parent_handle);
  StorageId from_parent_id =
      service_->GetStorageId(node_data.position.parent_handle.Get());
  MaybeAddModifiedParent(from_parent_id, node_data.position.parent_handle);
  default_observer_.OnChildMoved(to_position, node_data);
}

}  // namespace tabs
