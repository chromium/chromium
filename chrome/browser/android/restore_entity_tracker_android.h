// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RESTORE_ENTITY_TRACKER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_RESTORE_ENTITY_TRACKER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace tabs {

// Android-specific implementation for RestoreEntityTracker. Tracks and
// associates android-specific nodes with their storage IDs in the storage
// layer.
class RestoreEntityTrackerAndroid : public RestoreEntityTracker {
 public:
  using StorageLoadingContext = StorageLoadedData::StorageLoadingContext;

  RestoreEntityTrackerAndroid(
      OnTabAssociation on_tab_association,
      OnCollectionAssociation on_collection_association);
  ~RestoreEntityTrackerAndroid() override;

  void SetLoadingContext(StorageLoadingContext* context) override;

  void RegisterCollection(StorageId storage_id,
                          TabStorageType type,
                          const tabs_pb::Children& children,
                          std::optional<base::Token> collection_specific_id,
                          base::PassKey<TabStateStorageDatabase>) override;

  void RegisterTab(StorageId storage_id,
                   const tabs_pb::TabState& tab_state,
                   base::PassKey<TabStateStorageDatabase>) override;

  bool AssociateTab(const TabInterface* tab) override;
  bool AssociateCollection(const TabCollection* collection) override;
  bool HasCollectionBeenAssociated(TabCollection::Handle handle) override;

  bool HasNothingToAssociate() override;
  std::optional<StorageId> GetStorageIdForTab(int tab_android_id);

 private:
  bool AssociateTabStripCollection(const TabStripCollection* collection);
  bool AssociatePinnedCollection(const PinnedTabCollection* collection);
  bool AssociateUnpinnedCollection(const UnpinnedTabCollection* collection);
  bool AssociateTabGroupTabCollection(const TabGroupTabCollection* collection);
  bool AssociateSplitTabCollection(const SplitTabCollection* collection);

  // Helper method for associating a unique collection (tab strip, unpinned, or
  // pinned collection) with its storage ID.
  bool AssociateUniqueCollection(std::optional<StorageId> storage_id,
                                 const TabCollection* collection);

  // Helper method for associating a collection able to be uniquely identified
  // by a collection-specific token-based ID with its storage ID.
  // `id_to_storage_id` is a map of collection-specific IDs to storage IDs.
  bool AssociateCollectionUsingId(
      absl::flat_hash_map<base::Token, StorageId> id_to_storage_id,
      base::Token collection_specific_id,
      const TabCollection* collection);

  absl::flat_hash_map<int, StorageId> tab_android_id_to_storage_id_;
  absl::flat_hash_map<base::Token, StorageId> tab_group_id_to_storage_id_;
  absl::flat_hash_map<base::Token, StorageId> split_tab_id_to_storage_id_;
  absl::flat_hash_set<TabCollectionNodeHandle> associated_nodes_;

  std::optional<StorageId> pinned_collection_id_;
  std::optional<StorageId> unpinned_collection_id_;
  std::optional<StorageId> tab_strip_collection_id_;

  OnTabAssociation on_tab_association_;
  OnCollectionAssociation on_collection_association_;
  raw_ptr<StorageLoadingContext> context_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_RESTORE_ENTITY_TRACKER_ANDROID_H_
