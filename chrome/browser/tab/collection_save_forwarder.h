// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_COLLECTION_SAVE_FORWARDER_H_
#define CHROME_BROWSER_TAB_COLLECTION_SAVE_FORWARDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_strip_collection.h"
namespace tabs {

// Used to forward save requests to the TabStateStorageService.
class CollectionSaveForwarder {
 public:
  CollectionSaveForwarder(TabCollection* collection,
                          TabStateStorageService* service);
  ~CollectionSaveForwarder();

  CollectionSaveForwarder(const CollectionSaveForwarder&) = delete;
  CollectionSaveForwarder& operator=(const CollectionSaveForwarder&) = delete;

  static std::unique_ptr<CollectionSaveForwarder>
  CreateForTabGroupTabCollection(tab_groups::TabGroupId group_id,
                                 TabStripCollection* collection,
                                 TabStateStorageService* service);

  void Save();

 private:
  raw_ptr<TabStateStorageService> service_;
  raw_ptr<TabCollection> collection_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_COLLECTION_SAVE_FORWARDER_H_
