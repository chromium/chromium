// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_restore_orchestrator.h"

#include <cstddef>
#include <memory>

#include "base/check.h"
#include "base/types/pass_key.h"
#include "chrome/browser/tab/collection_storage_observer.h"
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/storage_collection_synchronizer.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "chrome/browser/tab/tab_storage_type.h"
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

// Recursively crawls the entire tree and associates or saves changes made prior
// to restore orchestration. The traversal order is determined by
// DirectChildWalker.
class PreRestoreChangesCrawler : public DirectChildWalker::Processor {
 public:
  PreRestoreChangesCrawler(RestoreEntityTracker* tracker,
                           TabStateStorageService* service,
                           base::PassKey<StorageRestoreOrchestrator> passkey)
      : tracker_(tracker), service_(service), passkey_(passkey) {}

  void ProcessTab(const TabInterface* tab) override {
    if (!tracker_->AssociateTab(tab)) {
      service_->Save(tab);
      service_->SaveDivergentChildren(tab->GetParentCollection(), passkey_);
    }
  }

  void ProcessCollection(const TabCollection* collection) override {
    DirectChildWalker walker(collection, this);
    walker.Walk();
    if (!tracker_->AssociateCollection(collection) &&
        ShouldSaveOnRestoreStart(collection)) {
      service_->Save(collection);
      service_->SaveDivergentChildren(collection, passkey_);
      service_->SaveDivergentChildren(collection->GetParentCollection(),
                                      passkey_);
    }
  }

  bool ShouldSaveOnRestoreStart(const TabCollection* collection) {
    TabStorageType type = TabCollectionTypeToTabStorageType(collection->type());
    return type != TabStorageType::kPinned && type != TabStorageType::kUnpinned;
  }

 private:
  raw_ptr<RestoreEntityTracker> tracker_;
  raw_ptr<TabStateStorageService> service_;
  base::PassKey<StorageRestoreOrchestrator> passkey_;
};

// ObserverImpl implementation.
StorageRestoreOrchestrator::ObserverImpl::ObserverImpl(
    StorageRestoreOrchestrator* orchestrator)
    : orchestrator_(orchestrator) {}

StorageRestoreOrchestrator::ObserverImpl::~ObserverImpl() = default;

void StorageRestoreOrchestrator::ObserverImpl::OnNodeRejected(StorageId node) {
  orchestrator_->OnNodeRejected(node);
}

// Post-order crawler for structural snapshots during restoration.
class RestoreStructureSnapshotCrawler : public DirectChildWalker::Processor {
 public:
  RestoreStructureSnapshotCrawler(
      TabStateStorageService* service,
      RestoreEntityTracker* tracker,
      base::PassKey<StorageRestoreOrchestrator> passkey)
      : service_(service), tracker_(tracker), passkey_(passkey) {}

  void ProcessTab(const TabInterface* tab) override {
    if (!tracker_->AssociateTab(tab) && !tracker_->HasTabBeenAssociated(tab)) {
      service_->Save(tab);
    }
  }

  void ProcessCollection(const TabCollection* collection) override {
    DirectChildWalker walker(collection, this);
    walker.Walk();

    if (!tracker_->AssociateCollection(collection) &&
        !tracker_->HasCollectionBeenAssociated(collection->GetHandle())) {
      service_->Save(collection);
    } else {
      service_->SavePayload(collection);
      service_->SaveDivergentChildren(collection, passkey_);
    }
  }

 private:
  raw_ptr<TabStateStorageService> service_;
  raw_ptr<RestoreEntityTracker> tracker_;
  base::PassKey<StorageRestoreOrchestrator> passkey_;
};

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

  auto batch = service_->CreateScopedBatch();
  RestoreEntityTracker* tracker = loaded_data_->GetTracker();
  // This is required to save the unique collections that do not emit observable
  // events on creation.
  if (tracker->HasNothingToAssociate()) {
    SaveStructureSnapshot(/*clear_unused_nodes=*/false);
  } else {
    tracker->AssociateCollection(collection_);
    PreRestoreChangesCrawler crawler(
        tracker, service_, base::PassKey<StorageRestoreOrchestrator>());
    DirectChildWalker walker(collection_, &crawler);
    walker.Walk();
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

void StorageRestoreOrchestrator::SetSynchronizer(
    StorageCollectionSynchronizer* synchronizer) {
  synchronizer_ = synchronizer;
}

void StorageRestoreOrchestrator::OnNodeRejected(StorageId node) {
  service_->Remove(node);
  if (synchronizer_) {
    synchronizer_->SaveStructureSnapshot(/*clear_unused_nodes=*/false,
                                         base::NullCallback());
  }
}

void StorageRestoreOrchestrator::OnRestoreCancelled() {
  is_restore_cancelled_ = true;
}

void StorageRestoreOrchestrator::OnChildrenAdded(
    const TabCollection::Position& position,
    const TabCollectionNodes& handles,
    bool insert_from_detached) {
  if (synchronizer_) {
    synchronizer_->SaveStructureSnapshot(/*clear_unused_nodes=*/false,
                                         base::NullCallback());
  }
}

void StorageRestoreOrchestrator::OnChildrenRemoved(
    const TabCollection::Position& position,
    const TabCollectionNodes& handles) {
  auto batch = service_->CreateScopedBatch();
  RestoreEntityTracker* tracker = loaded_data_->GetTracker();
  for (const auto& handle : handles) {
    if (std::holds_alternative<TabCollection::Handle>(handle)) {
      const TabCollection* collection =
          std::get<TabCollection::Handle>(handle).Get();
      tracker->AssociateCollection(collection);
      CollectionStorageObserver::ClearSubTree(collection, service_);
    } else if (std::holds_alternative<TabHandle>(handle)) {
      const TabInterface* tab = std::get<TabHandle>(handle).Get();
      tracker->AssociateTab(tab);
      service_->Remove(tab);
    }
  }
  if (synchronizer_) {
    synchronizer_->SaveStructureSnapshot(/*clear_unused_nodes=*/false,
                                         base::NullCallback());
  }
}

void StorageRestoreOrchestrator::OnChildMoved(
    const TabCollection::Position& to_position,
    const NodeData& node_data) {
  if (synchronizer_) {
    synchronizer_->SaveStructureSnapshot(/*clear_unused_nodes=*/false,
                                         base::NullCallback());
  }
}

void StorageRestoreOrchestrator::SaveChildNodeOnly(
    TabCollectionNodeHandle handle) {
  RestoreEntityTracker* tracker = loaded_data_->GetTracker();
  if (std::holds_alternative<TabHandle>(handle)) {
    const TabInterface* tab = std::get<TabHandle>(handle).Get();
    if (tab->GetParentCollection()) {
      tracker->AssociateTab(tab);
      service_->Save(tab);
    }
  } else if (std::holds_alternative<TabCollection::Handle>(handle)) {
    const TabCollection* collection =
        std::get<TabCollection::Handle>(handle).Get();
    if (collection->GetParentCollection()) {
      if (!tracker->AssociateCollection(collection) &&
          !tracker->HasCollectionBeenAssociated(collection->GetHandle())) {
        service_->Save(collection);
      } else {
        service_->SavePayload(collection);
      }
    }
  }
  if (synchronizer_) {
    synchronizer_->SaveStructureSnapshot(/*clear_unused_nodes=*/false,
                                         base::NullCallback());
  }
}

void StorageRestoreOrchestrator::SaveStructureSnapshot(
    bool clear_unused_nodes) {
  DCHECK(!clear_unused_nodes);
  if (is_restore_cancelled_) {
    return;
  }
  auto batch = service_->CreateScopedBatch();
  RestoreStructureSnapshotCrawler crawler(
      service_, loaded_data_->GetTracker(),
      base::PassKey<StorageRestoreOrchestrator>());
  crawler.ProcessCollection(collection_);
}

}  // namespace tabs
