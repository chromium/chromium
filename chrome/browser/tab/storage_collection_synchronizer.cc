// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_collection_synchronizer.h"

#include <memory>
#include <variant>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/tab/collection_storage_observer.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/direct_child_walker.h"
#include "components/tabs/public/tab_collection_storage.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

// Recursively crawls the entire tree and saves all children to the service. The
// traversal order is determined by DirectChildWalker.
class CollectionSaveCrawler : public DirectChildWalker::Processor {
 public:
  explicit CollectionSaveCrawler(TabStateStorageService* service)
      : service_(service) {}

  void ProcessTab(const TabInterface* tab) override { service_->Save(tab); }

  void ProcessCollection(const TabCollection* collection) override {
    service_->Save(collection);
    DirectChildWalker walker(collection, this);
    walker.Walk();
  }

 private:
  raw_ptr<TabStateStorageService> service_;
};

StorageCollectionSynchronizer::StorageCollectionSynchronizer(
    TabStripCollection* collection,
    TabStateStorageService* service)
    : collection_(collection), service_(service) {}

void StorageCollectionSynchronizer::FullSave() {
  CollectionSaveCrawler crawler(service_);
  DirectChildWalker walker(collection_, &crawler);
  walker.Walk();
}

void StorageCollectionSynchronizer::SetCollectionObserver(
    std::unique_ptr<TabCollectionObserver> new_observer) {
  if (observer_) {
    collection_->RemoveObserver(observer_.get());
  }
  observer_ = std::move(new_observer);
  if (observer_) {
    collection_->AddObserver(observer_.get());
  }
}

StorageCollectionSynchronizer::~StorageCollectionSynchronizer() {
  if (observer_) {
    collection_->RemoveObserver(observer_.get());
  }
}

}  // namespace tabs
