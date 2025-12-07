// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RESTORE_ENTITY_TRACKER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_RESTORE_ENTITY_TRACKER_ANDROID_H_

#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace tabs {

// Android-specific implementation for RestoreEntityTracker. Tracks and
// associates android-specific nodes with their storage IDs in the storage
// layer.
class RestoreEntityTrackerAndroid : public RestoreEntityTracker {
 public:
  RestoreEntityTrackerAndroid(
      OnTabAssociation on_tab_association,
      OnCollectionAssociation on_collection_association);
  ~RestoreEntityTrackerAndroid() override;

  void RegisterCollection(StorageId storage_id,
                          TabStorageType type,
                          const tabs_pb::Children& children,
                          base::PassKey<TabStateStorageDatabase>) override;

  void RegisterTab(StorageId storage_id,
                   const tabs_pb::TabState& tab_state,
                   base::PassKey<TabStateStorageDatabase>) override;

  bool AssociateTabAndAncestors(const TabInterface*) override;
  void AssociatePinnedCollection(const PinnedTabCollection*) override;
  bool HasCollectionBeenAssociated(TabCollection::Handle) override;

 private:
  // Associates a loaded TabCollection and its ancestors with their respective
  // storage IDs. This method should be called when a TabCollection is
  // instantiated using data from storage.
  void AssociateAncestorsInternal(StorageId storage_id,
                                  const TabCollection* collection);

  // Checks to see if a node should be processed. Ensures we do not process
  // previously associated nodes or unmapped nodes.
  bool ShouldProcessCollection(StorageId storage_id,
                               TabCollection::Handle collection);

  absl::flat_hash_map<StorageId, StorageId> id_to_parent_id_;
  absl::flat_hash_map<int, StorageId> tab_android_id_to_storage_id_;
  absl::flat_hash_set<TabCollection::Handle> associated_collections_;

  std::optional<StorageId> pinned_collection_id_;

  OnTabAssociation on_tab_association_;
  OnCollectionAssociation on_collection_association_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_RESTORE_ENTITY_TRACKER_ANDROID_H_
