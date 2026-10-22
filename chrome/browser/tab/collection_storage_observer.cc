// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/collection_storage_observer.h"

#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/direct_child_walker.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

// Post-order crawler for post-restore structural snapshots.
class StructureSnapshotCrawler : public DirectChildWalker::Processor {
 public:
  StructureSnapshotCrawler(TabStateStorageService* service,
                           std::vector<StorageId>& live_ids)
      : service_(service), live_ids_(live_ids) {}

  void ProcessTab(const TabInterface* tab) override {
    live_ids_->push_back(service_->GetStorageId(tab));
  }

  void ProcessCollection(const TabCollection* collection) override {
    DirectChildWalker walker(collection, this);
    walker.Walk();

    service_->Save(collection);
    live_ids_->push_back(service_->GetStorageId(collection));
  }

 private:
  raw_ptr<TabStateStorageService> service_;
  const raw_ref<std::vector<StorageId>> live_ids_;
};

// Traverses a TabCollection subtree and removes all encountered collections
// and tabs from the provided TabStateStorageService.
class SubTreeRemoveCrawler : public DirectChildWalker::Processor {
 public:
  SubTreeRemoveCrawler(const TabCollection* collection,
                       TabStateStorageService* service)
      : root_(collection), service_(service) {}

  void ProcessTab(const TabInterface* tab) override { service_->Remove(tab); }

  void ProcessCollection(const TabCollection* collection) override {
    service_->Remove(collection);
    DirectChildWalker walker(collection, this);
    walker.Walk();
  }

 private:
  raw_ptr<const TabCollection> root_;
  raw_ptr<TabStateStorageService> service_;
};

CollectionStorageObserver::CollectionStorageObserver(
    TabStripCollection* collection,
    TabStateStorageService* service)
    : collection_(collection), service_(service) {}

void CollectionStorageObserver::SaveStructureSnapshot(bool clear_unused_nodes) {
  auto batch = service_->CreateScopedBatch();
  std::vector<StorageId> live_ids;
  StructureSnapshotCrawler crawler(service_, live_ids);
  DirectChildWalker walker(collection_, &crawler);
  walker.Walk();

  service_->Save(collection_);
  live_ids.push_back(service_->GetStorageId(collection_));

  std::string window_tag = service_->GetWindowTag(collection_);
  bool is_off_the_record = service_->IsOffTheRecord(collection_);

  service_->ClearDivergentNodesForWindow(window_tag, is_off_the_record);
  if (clear_unused_nodes) {
    service_->ClearNodesForWindowExcept(window_tag, is_off_the_record,
                                        std::move(live_ids));
  }
}

void CollectionStorageObserver::SetSynchronizer(
    StorageCollectionSynchronizer* synchronizer) {
  synchronizer_ = synchronizer;
}

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
  if (synchronizer_) {
    synchronizer_->SaveStructureSnapshot(/*clear_unused_nodes=*/false,
                                         base::NullCallback());
  }
}

void CollectionStorageObserver::OnChildrenAdded(
    const TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles,
    bool insert_from_detached) {
  for (const auto& handle : handles) {
    if (std::holds_alternative<TabHandle>(handle)) {
      const TabInterface* tab = std::get<TabHandle>(handle).Get();
      service_->Save(tab);
    }
  }
  if (synchronizer_) {
    synchronizer_->SaveStructureSnapshot(/*clear_unused_nodes=*/false,
                                         base::NullCallback());
  }
}

void CollectionStorageObserver::OnChildrenRemoved(
    const TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles) {
  for (const auto& handle : handles) {
    if (std::holds_alternative<TabCollection::Handle>(handle)) {
      const TabCollection* collection =
          std::get<TabCollection::Handle>(handle).Get();
      ClearSubTree(collection, service_);
    } else {
      const TabInterface* tab = std::get<TabHandle>(handle).Get();
      service_->Remove(tab);
    }
  }
  if (synchronizer_) {
    synchronizer_->SaveStructureSnapshot(/*clear_unused_nodes=*/false,
                                         base::NullCallback());
  }
}

void CollectionStorageObserver::OnChildMoved(
    const TabCollection::Position& to_position,
    const NodeData& node_data) {
  if (synchronizer_) {
    synchronizer_->SaveStructureSnapshot(/*clear_unused_nodes=*/false,
                                         base::NullCallback());
  }
}

// static
void CollectionStorageObserver::ClearSubTree(const TabCollection* collection,
                                             TabStateStorageService* service) {
  service->Remove(collection);
  SubTreeRemoveCrawler crawler(collection, service);
  DirectChildWalker walker(collection, &crawler);
  walker.Walk();
}

}  // namespace tabs
