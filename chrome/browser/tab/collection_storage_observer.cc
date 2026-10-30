// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/collection_storage_observer.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/tab/node_hash_cache.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/direct_child_walker.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

namespace {

void ProcessAndSaveCollection(const TabCollection* collection,
                              TabStateStorageService* service,
                              NodeHashCache* hash_cache,
                              std::vector<StorageId>& live_ids) {
  StorageId storage_id = service->GetStorageId(collection);
  live_ids.push_back(storage_id);

  if (!hash_cache) {
    service->Save(collection);
    return;
  }

  const bool is_tab_strip =
      (collection->type() == TabCollection::Type::TABSTRIP);
  NodeHashes current_hashes =
      NodeHashCache::ComputeHashes(collection, *service);
  std::optional<NodeHashes> cached = hash_cache->GetNodeHashes(storage_id);

  if (!cached.has_value()) {
    service->Save(collection);
    hash_cache->SetNodeHashes(storage_id, current_hashes);
    return;
  }

  const bool payload_changed =
      is_tab_strip || (current_hashes.payload_hash != cached->payload_hash);
  const bool children_changed =
      (current_hashes.children_hash != cached->children_hash);

  if (payload_changed && children_changed) {
    service->Save(collection);
  } else if (payload_changed) {
    service->SavePayload(collection);
  } else if (children_changed) {
    service->SaveChildren(collection);
  }

  if (current_hashes != *cached) {
    hash_cache->SetNodeHashes(storage_id, current_hashes);
  }
}

// Post-order crawler for post-restore and live structural snapshots.
class StructureSnapshotCrawler : public DirectChildWalker::Processor {
 public:
  StructureSnapshotCrawler(TabStateStorageService* service,
                           NodeHashCache* hash_cache,
                           std::vector<StorageId>& live_ids)
      : service_(service), hash_cache_(hash_cache), live_ids_(live_ids) {}

  void ProcessTab(const TabInterface* tab) override {
    live_ids_->push_back(service_->GetStorageId(tab));
  }

  void ProcessCollection(const TabCollection* collection) override {
    DirectChildWalker walker(collection, this);
    walker.Walk();

    ProcessAndSaveCollection(collection, service_, hash_cache_, *live_ids_);
  }

 private:
  raw_ptr<TabStateStorageService> service_;
  raw_ptr<NodeHashCache> hash_cache_;
  const raw_ref<std::vector<StorageId>> live_ids_;
};

// Traverses a TabCollection subtree and removes all encountered collections
// and tabs from the provided TabStateStorageService and NodeHashCache.
class SubTreeRemoveCrawler : public DirectChildWalker::Processor {
 public:
  SubTreeRemoveCrawler(TabStateStorageService* service,
                       NodeHashCache* hash_cache)
      : service_(service), hash_cache_(hash_cache) {}

  void ProcessTab(const TabInterface* tab) override { service_->Remove(tab); }

  void ProcessCollection(const TabCollection* collection) override {
    service_->Remove(collection);
    if (hash_cache_) {
      hash_cache_->RemoveNode(service_->GetStorageId(collection));
    }
    DirectChildWalker walker(collection, this);
    walker.Walk();
  }

 private:
  raw_ptr<TabStateStorageService> service_;
  raw_ptr<NodeHashCache> hash_cache_;
};

}  // namespace

CollectionStorageObserver::CollectionStorageObserver(
    TabStripCollection* collection,
    TabStateStorageService* service)
    : collection_(collection), service_(service) {}

void CollectionStorageObserver::SaveStructureSnapshot(bool clear_unused_nodes) {
  auto batch = service_->CreateScopedBatch();
  std::vector<StorageId> live_ids;
  NodeHashCache* hash_cache =
      synchronizer_ ? &synchronizer_->hash_cache() : nullptr;

  StructureSnapshotCrawler crawler(service_, hash_cache, live_ids);
  DirectChildWalker walker(collection_, &crawler);
  walker.Walk();

  ProcessAndSaveCollection(collection_, service_, hash_cache, live_ids);

  std::string window_tag = service_->GetWindowTag(collection_);
  bool is_off_the_record = service_->IsOffTheRecord(collection_);

  service_->ClearDivergentNodesForWindow(window_tag, is_off_the_record);
  if (clear_unused_nodes) {
    if (hash_cache) {
      hash_cache->PruneExcept(live_ids);
    }
    service_->ClearNodesForWindowExcept(window_tag, is_off_the_record,
                                        std::move(live_ids));
  }
}

void CollectionStorageObserver::SetSynchronizer(
    StorageCollectionSynchronizer* synchronizer) {
  synchronizer_ = synchronizer;
}

CollectionStorageObserver::~CollectionStorageObserver() = default;

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
  NodeHashCache* hash_cache =
      synchronizer_ ? &synchronizer_->hash_cache() : nullptr;
  for (const auto& handle : handles) {
    if (std::holds_alternative<TabCollection::Handle>(handle)) {
      const TabCollection* collection =
          std::get<TabCollection::Handle>(handle).Get();
      ClearSubTree(collection, service_, hash_cache);
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
                                             TabStateStorageService* service,
                                             NodeHashCache* hash_cache) {
  SubTreeRemoveCrawler crawler(service, hash_cache);
  crawler.ProcessCollection(collection);
}

}  // namespace tabs
