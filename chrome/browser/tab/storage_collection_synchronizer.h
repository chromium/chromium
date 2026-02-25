// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_COLLECTION_SYNCHRONIZER_H_
#define CHROME_BROWSER_TAB_STORAGE_COLLECTION_SYNCHRONIZER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_collection_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

// Provides updates to storage to match the state of a TabStripCollection.
class StorageCollectionSynchronizer {
 public:
  class CollectionSynchronizerObserver : public TabCollectionObserver {
   public:
    // Saves the node represented by the handle to storage. This
    // will only save the payload for the given handle.
    virtual void SaveChildNodeOnly(TabCollectionNodeHandle handle) = 0;

    // Called when the restoration process is cancelled.
    virtual void OnRestoreCancelled();
  };

  StorageCollectionSynchronizer(TabStripCollection* collection,
                                TabStateStorageService* service);
  ~StorageCollectionSynchronizer();

  StorageCollectionSynchronizer(const StorageCollectionSynchronizer&) = delete;
  StorageCollectionSynchronizer& operator=(
      const StorageCollectionSynchronizer&) = delete;

  // Saves the entire collection and its descendants to the service.
  void FullSave();

  // Cancels the restoration process.
  void CancelRestore();

  // Used to manually save a tab.
  void SaveTab(TabInterface* tab);

  // Used to manually save a tab group payload.
  void SaveTabGroupPayload(tab_groups::TabGroupId group_id);

  // Sets the TabCollectionObserver. If an observer is already set, it will be
  // unregistered before the new one is registered.
  void SetCollectionObserver(
      std::unique_ptr<CollectionSynchronizerObserver> new_observer);

 private:
  raw_ptr<TabStripCollection> collection_;
  raw_ptr<TabStateStorageService> service_;
  std::unique_ptr<CollectionSynchronizerObserver> observer_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_COLLECTION_SYNCHRONIZER_H_
