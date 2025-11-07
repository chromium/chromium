// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RESTORE_ID_ASSOCIATOR_ANDROID_H_
#define CHROME_BROWSER_ANDROID_RESTORE_ID_ASSOCIATOR_ANDROID_H_

#include "chrome/browser/tab/restore_id_associator.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace tabs {

// Android-specific implementation for RestoreIdAssociator. Associates
// android-specific nodes with their storage IDs in the storage layer.
class RestoreIdAssociatorAndroid : public RestoreIdAssociator {
 public:
  struct RestoreIdAssociatorState {
    RestoreIdAssociatorState(OnTabAssociation, OnCollectionAssociation);
    ~RestoreIdAssociatorState();

    absl::flat_hash_map<int, int> id_to_parent_id;
    absl::flat_hash_map<int, int> tab_android_id_to_storage_id;
    absl::flat_hash_set<TabCollection::Handle> associated_collections;

    std::optional<int> pinned_collection_id;

    OnTabAssociation on_tab_association;
    OnCollectionAssociation on_collection_association;
  };

  explicit RestoreIdAssociatorAndroid(
      std::unique_ptr<RestoreIdAssociatorState>);
  ~RestoreIdAssociatorAndroid() override;

  bool AssociateTabAndAncestors(const TabInterface*) override;
  void AssociatePinnedCollection(const PinnedTabCollection*) override;

  bool HasCollectionBeenAssociated(TabCollection::Handle) override;

 private:
  // Associates a loaded TabCollection and its ancestors with their respective
  // storage IDs. This method should be called when a TabCollection is
  // instantiated using data from storage.
  void AssociateAncestorsInternal(int storage_id,
                                  const TabCollection* collection);

  // Checks to see if a node should be processed. Ensures we do not process
  // previously associated nodes or unmapped nodes.
  bool ShouldProcessCollection(int storage_id,
                               TabCollection::Handle collection);

  std::unique_ptr<RestoreIdAssociatorState> state_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_RESTORE_ID_ASSOCIATOR_ANDROID_H_
