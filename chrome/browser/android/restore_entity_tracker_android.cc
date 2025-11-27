// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/restore_entity_tracker_android.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/token.pb.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

RestoreEntityTrackerAndroid::RestoreEntityTrackerAndroid(
    OnTabAssociation on_tab_association,
    OnCollectionAssociation on_collection_association)
    : on_tab_association_(on_tab_association),
      on_collection_association_(on_collection_association) {}

RestoreEntityTrackerAndroid::~RestoreEntityTrackerAndroid() = default;

void RestoreEntityTrackerAndroid::RegisterCollection(
    StorageId storage_id,
    TabStorageType type,
    const tabs_pb::Children& children,
    base::PassKey<TabStateStorageDatabase>) {
  // Build a mapping of children IDs to parent IDs;
  for (const tabs_pb::Token& child_id : children.storage_id()) {
    id_to_parent_id_[StorageIdFromTokenProto(child_id)] = storage_id;
  }

  if (type == TabStorageType::kPinned) {
    DCHECK(!pinned_collection_id_) << "Should only have one pinned collection.";
    pinned_collection_id_ = storage_id;
  }
}

void RestoreEntityTrackerAndroid::RegisterTab(
    StorageId storage_id,
    const tabs_pb::TabState& tab_state,
    base::PassKey<TabStateStorageDatabase>) {
  tab_android_id_to_storage_id_[tab_state.tab_id()] = storage_id;
}

bool RestoreEntityTrackerAndroid::AssociateTabAndAncestors(
    const TabInterface* tab) {
  const TabAndroid* tab_android = static_cast<const TabAndroid*>(tab);
  auto it = tab_android_id_to_storage_id_.find(tab_android->GetAndroidId());
  if (it == tab_android_id_to_storage_id_.end()) {
    return false;
  }

  StorageId storage_id = it->second;
  on_tab_association_.Run(storage_id, tab_android);
  AssociateAncestorsInternal(id_to_parent_id_.at(storage_id),
                             tab->GetParentCollection());
  return true;
}

void RestoreEntityTrackerAndroid::AssociatePinnedCollection(
    const PinnedTabCollection* collection) {
  if (pinned_collection_id_) {
    on_collection_association_.Run(pinned_collection_id_.value(), collection);
    associated_collections_.insert(collection->GetHandle());
  }
}

bool RestoreEntityTrackerAndroid::HasCollectionBeenAssociated(
    TabCollection::Handle handle) {
  return associated_collections_.contains(handle);
}

void RestoreEntityTrackerAndroid::AssociateAncestorsInternal(
    StorageId storage_id,
    const TabCollection* collection) {
  TabCollection::Handle curr_collection = collection->GetHandle();
  StorageId curr_id = storage_id;

  while (ShouldProcessCollection(curr_id, curr_collection)) {
    const TabCollection* curr_collection_ptr = curr_collection.Get();
    on_collection_association_.Run(curr_id, curr_collection_ptr);
    associated_collections_.insert(curr_collection);

    // Handle root node.
    if (!curr_collection_ptr->GetParentCollection()) {
      return;
    }

    // Update curr node details.
    curr_collection = curr_collection_ptr->GetParentCollection()->GetHandle();
    curr_id = id_to_parent_id_.at(curr_id);
  }
}

bool RestoreEntityTrackerAndroid::ShouldProcessCollection(
    StorageId storage_id,
    TabCollection::Handle collection) {
  // Must allow for root collection.
  bool is_root = !collection.Get()->GetParentCollection();
  bool is_child = id_to_parent_id_.contains(storage_id);

  return !associated_collections_.contains(collection) && (is_root || is_child);
}

}  // namespace tabs
