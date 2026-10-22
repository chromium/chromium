// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_COLLECTION_STORAGE_OBSERVER_H_
#define CHROME_BROWSER_TAB_COLLECTION_STORAGE_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/tab/storage_collection_synchronizer.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_collection_observer.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

// Observes changes to the structure of a TabStripCollection.
// This class does not manage observer registration and
// unregistration.
class CollectionStorageObserver
    : public StorageCollectionSynchronizer::CollectionSynchronizerObserver {
 public:
  CollectionStorageObserver(TabStripCollection* collection,
                            TabStateStorageService* service);
  ~CollectionStorageObserver() override;

  CollectionStorageObserver(const CollectionStorageObserver&) = delete;
  CollectionStorageObserver& operator=(const CollectionStorageObserver&) =
      delete;

  // CollectionSynchronizerObserver Implementation:
  void OnChildrenAdded(const TabCollection::Position& position,
                       const TabCollectionNodes& handles,
                       bool insert_from_detached) override;
  void OnChildrenRemoved(const TabCollection::Position& position,
                         const TabCollectionNodes& handles) override;
  void OnChildMoved(const TabCollection::Position& to_position,
                    const NodeData& node_data) override;
  void SaveChildNodeOnly(TabCollectionNodeHandle handle) override;

  void SaveStructureSnapshot(bool clear_unused_nodes) override;
  void SetSynchronizer(StorageCollectionSynchronizer* synchronizer) override;

  // Removes `collection` and all its descendants from storage.
  static void ClearSubTree(const TabCollection* collection,
                           TabStateStorageService* service);

 private:
  raw_ptr<TabStripCollection> collection_;
  raw_ptr<TabStateStorageService> service_;
  raw_ptr<StorageCollectionSynchronizer> synchronizer_ = nullptr;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_COLLECTION_STORAGE_OBSERVER_H_
