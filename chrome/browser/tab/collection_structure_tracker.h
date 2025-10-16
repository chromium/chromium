// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_COLLECTION_STRUCTURE_TRACKER_H_
#define CHROME_BROWSER_TAB_COLLECTION_STRUCTURE_TRACKER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/collection_storage_observer.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/tab_strip_collection.h"
namespace tabs {

// Provides updates to storage to match the state of a TabStripCollection.
class CollectionStructureTracker {
 public:
  CollectionStructureTracker(TabStripCollection* collection,
                             TabStateStorageService* service);
  ~CollectionStructureTracker();

  CollectionStructureTracker(const CollectionStructureTracker&) = delete;
  CollectionStructureTracker& operator=(const CollectionStructureTracker&) =
      delete;

  // Saves the entire collection and its descendants to the service.
  void FullSave();

 private:
  raw_ptr<TabStripCollection> collection_;
  raw_ptr<TabStateStorageService> service_;
  std::unique_ptr<CollectionStorageObserver> observer_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_COLLECTION_STRUCTURE_TRACKER_H_
