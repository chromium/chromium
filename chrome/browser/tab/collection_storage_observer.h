// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_COLLECTION_STORAGE_OBSERVER_H_
#define CHROME_BROWSER_TAB_COLLECTION_STORAGE_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/tab_collection_observer.h"

namespace tabs {

// Observes changes to the structure of a TabStripCollection.
// This class does not manage observer registration and
// unregistration.
class CollectionStorageObserver : public TabCollectionObserver {
 public:
  explicit CollectionStorageObserver(TabStateStorageService* service);
  ~CollectionStorageObserver() override;

  CollectionStorageObserver(const CollectionStorageObserver&) = delete;
  CollectionStorageObserver& operator=(const CollectionStorageObserver&) =
      delete;

  // TabCollectionObserver Implementation:
  void OnChildrenAdded(const TabCollection::Position& position,
                       const TabCollectionNodes& handles,
                       bool insert_from_detached) override;
  void OnChildrenRemoved(const TabCollection::Position& position,
                         const TabCollectionNodes& handles) override;
  void OnChildMoved(const TabCollection::Position& to_position,
                    const NodeData& node_data) override;

 private:
  raw_ptr<TabStateStorageService> service_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_COLLECTION_STORAGE_OBSERVER_H_
