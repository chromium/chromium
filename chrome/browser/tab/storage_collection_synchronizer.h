// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_COLLECTION_SYNCHRONIZER_H_
#define CHROME_BROWSER_TAB_STORAGE_COLLECTION_SYNCHRONIZER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/collection_storage_observer.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/tab_collection_observer.h"
#include "components/tabs/public/tab_strip_collection.h"
namespace tabs {

// Provides updates to storage to match the state of a TabStripCollection.
class StorageCollectionSynchronizer {
 public:
  StorageCollectionSynchronizer(TabStripCollection* collection,
                                TabStateStorageService* service);
  ~StorageCollectionSynchronizer();

  StorageCollectionSynchronizer(const StorageCollectionSynchronizer&) = delete;
  StorageCollectionSynchronizer& operator=(
      const StorageCollectionSynchronizer&) = delete;

  // Saves the entire collection and its descendants to the service.
  void FullSave();

  // Sets the TabCollectionObserver. If an observer is already set, it will be
  // unregistered before the new one is registered.
  void SetCollectionObserver(
      std::unique_ptr<TabCollectionObserver> new_observer);

 private:
  raw_ptr<TabStripCollection> collection_;
  raw_ptr<TabStateStorageService> service_;
  std::unique_ptr<TabCollectionObserver> observer_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_COLLECTION_SYNCHRONIZER_H_
