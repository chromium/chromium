// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/restore_id_associator_android.h"

#include <memory>
#include <utility>

#include "chrome/browser/android/tab_android.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

RestoreIdAssociatorAndroid::RestoreIdAssociatorState::RestoreIdAssociatorState(
    OnTabAssociation on_tab_association,
    OnCollectionAssociation on_collection_association)
    : on_tab_association(on_tab_association),
      on_collection_association(on_collection_association) {}

RestoreIdAssociatorAndroid::RestoreIdAssociatorState::
    ~RestoreIdAssociatorState() = default;

RestoreIdAssociatorAndroid::RestoreIdAssociatorAndroid(
    std::unique_ptr<RestoreIdAssociatorState> state)
    : state_(std::move(state)) {}
RestoreIdAssociatorAndroid::~RestoreIdAssociatorAndroid() = default;

bool RestoreIdAssociatorAndroid::AssociateTabAndAncestors(
    const TabInterface* tab) {
  const TabAndroid* tab_android = static_cast<const TabAndroid*>(tab);
  if (!state_->tab_android_id_to_storage_id.contains(
          tab_android->GetAndroidId())) {
    return false;
  }

  int storage_id =
      state_->tab_android_id_to_storage_id[tab_android->GetAndroidId()];
  state_->on_tab_association.Run(storage_id, tab_android);
  AssociateAncestorsInternal(state_->id_to_parent_id.at(storage_id),
                             tab->GetParentCollection());
  return true;
}

void RestoreIdAssociatorAndroid::AssociatePinnedCollection(
    const PinnedTabCollection* collection) {
  if (state_->pinned_collection_id) {
    state_->on_collection_association.Run(state_->pinned_collection_id.value(),
                                          collection);
    state_->associated_collections.insert(collection->GetHandle());
  }
}

bool RestoreIdAssociatorAndroid::HasCollectionBeenAssociated(
    TabCollection::Handle handle) {
  return state_->associated_collections.contains(handle);
}

void RestoreIdAssociatorAndroid::AssociateAncestorsInternal(
    int storage_id,
    const TabCollection* collection) {
  TabCollection::Handle curr_collection = collection->GetHandle();
  int curr_id = storage_id;

  while (ShouldProcessCollection(curr_id, curr_collection)) {
    const TabCollection* curr_collection_ptr = curr_collection.Get();
    state_->on_collection_association.Run(curr_id, curr_collection_ptr);
    state_->associated_collections.insert(curr_collection);

    // Handle root node.
    if (!curr_collection_ptr->GetParentCollection()) {
      return;
    }

    // Update curr node details.
    curr_collection = curr_collection_ptr->GetParentCollection()->GetHandle();
    curr_id = state_->id_to_parent_id.at(curr_id);
  }
}

bool RestoreIdAssociatorAndroid::ShouldProcessCollection(
    int storage_id,
    TabCollection::Handle collection) {
  // Must allow for root collection.
  bool is_root = !collection.Get()->GetParentCollection();
  bool is_child = state_->id_to_parent_id.contains(storage_id);

  return !state_->associated_collections.contains(collection) &&
         (is_root || is_child);
}

}  // namespace tabs
