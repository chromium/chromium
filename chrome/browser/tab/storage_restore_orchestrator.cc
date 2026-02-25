// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_restore_orchestrator.h"

#include <cstddef>
#include <memory>

#include "base/check.h"
#include "chrome/browser/tab/collection_storage_observer.h"
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/storage_collection_synchronizer.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "chrome/browser/tab/tab_storage_util.h"
#include "components/tabs/public/direct_child_walker.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/unpinned_tab_collection.h"

namespace tabs {

// Recursively crawls the entire tree and saves all descendants' child vectors
// to the service. The traversal order is determined by DirectChildWalker.
class CollectionChildSaveCrawler : public DirectChildWalker::Processor {
 public:
  explicit CollectionChildSaveCrawler(TabStateStorageService* service)
      : service_(service) {}

  void ProcessTab(const TabInterface* tab) override {}

  void ProcessCollection(const TabCollection* collection) override {
    service_->SaveChildren(collection);
    DirectChildWalker walker(collection, this);
    walker.Walk();
  }

 private:
  raw_ptr<TabStateStorageService> service_;
};

// ObserverImpl implementation.
StorageRestoreOrchestrator::ObserverImpl::ObserverImpl(
    StorageRestoreOrchestrator* orchestrator)
    : orchestrator_(orchestrator) {}

StorageRestoreOrchestrator::ObserverImpl::~ObserverImpl() = default;

void StorageRestoreOrchestrator::ObserverImpl::OnNodeRejected(StorageId node) {
  orchestrator_->OnNodeRejected(node);
}

// StorageRestoreOrchestrator implementation.
StorageRestoreOrchestrator::StorageRestoreOrchestrator(
    TabStripCollection* collection,
    TabStateStorageService* service,
    StorageLoadedData* loaded_data)
    : data_observer_(this),
      collection_(collection),
      service_(service),
      loaded_data_(loaded_data) {
  loaded_data_->RegisterObserver(&data_observer_);

  RestoreEntityTracker* tracker = loaded_data_->GetTracker();
  // This is required to save the unique collections that do not emit observable
  // events on creation.
  if (tracker->HasNothingToAssociate()) {
    StorageCollectionSynchronizer synchronizer(collection_, service_);
    synchronizer.FullSave();
  } else {
    tracker->AssociateCollection(collection_);
    tracker->AssociateCollection(collection_->pinned_collection());
    tracker->AssociateCollection(collection_->unpinned_collection());
  }
}

StorageRestoreOrchestrator::~StorageRestoreOrchestrator() {
  DCHECK(loaded_data_)
      << "StorageLoadedData must be alive when the orchestrator is destroyed.";
  loaded_data_->UnregisterObserver(&data_observer_);

  if (!is_restore_cancelled_) {
    auto batch = service_->CreateScopedBatch();
    service_->ClearDivergentNodesForWindow(loaded_data_->GetWindowTag(),
                                           loaded_data_->IsOffTheRecord());
    service_->SaveChildren(collection_);
    CollectionChildSaveCrawler crawler(service_);
    DirectChildWalker walker(collection_, &crawler);
    walker.Walk();
  }
}

void StorageRestoreOrchestrator::OnSaveChildTab(
    const TabCollection::NodeHandle& handle,
    bool was_inserted) {
  if (!std::holds_alternative<TabHandle>(handle)) {
    return;
  }
  const TabInterface* tab = std::get<TabHandle>(handle).Get();
  const TabCollection* parent = tab->GetParentCollection();
  if (!parent) {
    return;
  }

  RestoreEntityTracker* tracker = loaded_data_->GetTracker();
  if (was_inserted && tracker->AssociateTab(tab)) {
    TabCollectionHandle parent_handle = parent->GetHandle();
    DCHECK(tracker->HasCollectionBeenAssociated(parent_handle));
  } else {
    // Nodes not previously persisted will be marked as divergent.
    service_->Save(tab);
  }

  service_->SaveDivergentChildren(parent,
                                  base::PassKey<StorageRestoreOrchestrator>());
}

void StorageRestoreOrchestrator::OnSaveChildCollection(
    const TabCollection::NodeHandle& handle,
    bool was_inserted) {
  if (!std::holds_alternative<TabCollection::Handle>(handle)) {
    return;
  }

  TabCollection::Handle collection_handle =
      std::get<TabCollection::Handle>(handle);
  const TabCollection* collection = collection_handle.Get();
  const TabCollection* parent = collection->GetParentCollection();
  if (!parent) {
    return;
  }

  RestoreEntityTracker* tracker = loaded_data_->GetTracker();
  if (was_inserted && tracker->AssociateCollection(collection)) {
    TabCollectionHandle parent_handle = parent->GetHandle();
    DCHECK(tracker->HasCollectionBeenAssociated(parent_handle));
  } else {
    // Nodes not previously persisted will be marked as divergent.
    service_->Save(collection);
  }

  service_->SaveDivergentChildren(parent,
                                  base::PassKey<StorageRestoreOrchestrator>());
}

void StorageRestoreOrchestrator::OnNodeRejected(StorageId node) {
  service_->Remove(node);
}

void StorageRestoreOrchestrator::OnRestoreCancelled() {
  is_restore_cancelled_ = true;
}

void StorageRestoreOrchestrator::OnChildrenAdded(
    const TabCollection::Position& position,
    const TabCollectionNodes& handles,
    bool insert_from_detached) {
  // Associating a tab also associates its ancestors.
  for (const auto& handle : handles) {
    OnSaveChildTab(handle, /*was_inserted=*/true);
  }

  // Any collection not already associated by the tab-filtered pass is new and
  // must be saved.
  for (const auto& handle : handles) {
    OnSaveChildCollection(handle, /*was_inserted=*/true);
  }
}

void StorageRestoreOrchestrator::OnChildrenRemoved(
    const TabCollection::Position& position,
    const TabCollectionNodes& handles) {
  // Cannot remove without adding first, so we can assume that these nodes are
  // already associated.
  for (const auto& handle : handles) {
    if (std::holds_alternative<TabCollection::Handle>(handle)) {
      service_->Remove(std::get<TabCollection::Handle>(handle).Get());
    } else if (std::holds_alternative<TabHandle>(handle)) {
      service_->Remove(std::get<TabHandle>(handle).Get());
    }
  }
  service_->SaveDivergentChildren(position.parent_handle.Get(),
                                  base::PassKey<StorageRestoreOrchestrator>());
}

void StorageRestoreOrchestrator::OnChildMoved(
    const TabCollection::Position& to_position,
    const NodeData& node_data) {
  service_->SaveDivergentChildren(to_position.parent_handle.Get(),
                                  base::PassKey<StorageRestoreOrchestrator>());
  service_->SaveDivergentChildren(node_data.position.parent_handle.Get(),
                                  base::PassKey<StorageRestoreOrchestrator>());
}

void StorageRestoreOrchestrator::SaveChildNodeOnly(TabCollectionNodeHandle handle) {
  if (std::holds_alternative<TabHandle>(handle)) {
    OnSaveChildTab(handle, /*was_inserted=*/false);
  } else {
    OnSaveChildCollection(handle, /*was_inserted=*/false);
  }
}

}  // namespace tabs
