// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/restore_entity_tracker_android.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_android_conversions.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/token.pb.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "chrome/browser/tab/tab_storage_util.h"

namespace tabs {

RestoreEntityTrackerAndroid::RestoreEntityTrackerAndroid(
    OnTabAssociation on_tab_association,
    OnCollectionAssociation on_collection_association)
    : on_tab_association_(on_tab_association),
      on_collection_association_(on_collection_association) {}

RestoreEntityTrackerAndroid::~RestoreEntityTrackerAndroid() = default;

void RestoreEntityTrackerAndroid::SetLoadingContext(
    StorageLoadingContext* context) {
  context_ = context;
}

void RestoreEntityTrackerAndroid::RegisterCollection(
    StorageId storage_id,
    TabStorageType type,
    const tabs_pb::Children& children,
    std::optional<base::Token> collection_specific_id,
    base::PassKey<TabStateStorageDatabase>) {
  DCHECK(context_);
  if (context_->HasError()) {
    return;
  }

  if (type == TabStorageType::kPinned) {
    if (pinned_collection_id_) {
      context_->SetStatus(StorageLoadingStatus::kMultipleUniqueNodesError,
                          "Should only have one pinned collection.");
      return;
    }
    pinned_collection_id_ = storage_id;
  } else if (type == TabStorageType::kUnpinned) {
    if (unpinned_collection_id_) {
      context_->SetStatus(StorageLoadingStatus::kMultipleUniqueNodesError,
                          "Should only have one unpinned collection.");
      return;
    }
    unpinned_collection_id_ = storage_id;
  } else if (type == TabStorageType::kTabStrip) {
    if (tab_strip_collection_id_) {
      context_->SetStatus(StorageLoadingStatus::kMultipleUniqueNodesError,
                          "Should only have one tab strip collection.");
      return;
    }
    tab_strip_collection_id_ = storage_id;
  } else if (type == TabStorageType::kSplit) {
    DCHECK(collection_specific_id.has_value());
    split_tab_id_to_storage_id_[*collection_specific_id] = storage_id;
  } else if (type == TabStorageType::kGroup) {
    DCHECK(collection_specific_id.has_value());
    tab_group_id_to_storage_id_[*collection_specific_id] = storage_id;
  }
}

void RestoreEntityTrackerAndroid::RegisterTab(
    StorageId storage_id,
    const tabs_pb::TabState& tab_state,
    base::PassKey<TabStateStorageDatabase>) {
  DCHECK(context_);
  if (context_->HasError()) {
    return;
  }
  tab_android_id_to_storage_id_[tab_state.tab_id()] = storage_id;
}

bool RestoreEntityTrackerAndroid::AssociateTab(const TabInterface* tab) {
  DCHECK(context_);
  if (context_->HasError()) {
    return false;
  }

  const TabAndroid* tab_android = ToTabAndroidChecked(tab);
  TabHandle handle = tab->GetHandle();
  if (associated_nodes_.contains(handle)) {
    return false;
  }

  auto it = tab_android_id_to_storage_id_.find(tab_android->GetAndroidId());
  if (it == tab_android_id_to_storage_id_.end()) {
    return false;
  }

  StorageId storage_id = it->second;
  on_tab_association_.Run(storage_id, tab_android);
  associated_nodes_.insert(handle);
  return true;
}

bool RestoreEntityTrackerAndroid::AssociateCollection(
    const TabCollection* collection) {
  DCHECK(context_);
  if (context_->HasError()) {
    return false;
  }

  TabStorageType type = TabCollectionTypeToTabStorageType(collection->type());
  if (type == TabStorageType::kPinned) {
    return AssociatePinnedCollection(
        static_cast<const PinnedTabCollection*>(collection));
  } else if (type == TabStorageType::kUnpinned) {
    return AssociateUnpinnedCollection(
        static_cast<const UnpinnedTabCollection*>(collection));
  } else if (type == TabStorageType::kTabStrip) {
    return AssociateTabStripCollection(
        static_cast<const TabStripCollection*>(collection));
  } else if (type == TabStorageType::kSplit) {
    return AssociateSplitTabCollection(
        static_cast<const SplitTabCollection*>(collection));
  } else if (type == TabStorageType::kGroup) {
    return AssociateTabGroupTabCollection(
        static_cast<const TabGroupTabCollection*>(collection));
  } else {
    context_->SetStatus(StorageLoadingStatus::kUnknownCollectionTypeError,
                        "Unknown collection type: " +
                            base::NumberToString(static_cast<int>(type)));
    return false;
  }
}

bool RestoreEntityTrackerAndroid::AssociateUniqueCollection(
    std::optional<StorageId> storage_id,
    const TabCollection* collection) {
  TabCollection::Handle handle = collection->GetHandle();
  if (storage_id && !associated_nodes_.contains(handle)) {
    on_collection_association_.Run(storage_id.value(), collection);
    associated_nodes_.insert(handle);
    return true;
  }
  return false;
}

bool RestoreEntityTrackerAndroid::AssociateCollectionUsingId(
    absl::flat_hash_map<base::Token, StorageId> id_to_storage_id,
    base::Token collection_specific_id,
    const TabCollection* collection) {
  TabCollection::Handle handle = collection->GetHandle();
  if (associated_nodes_.contains(handle)) {
    return false;
  }

  auto it = id_to_storage_id.find(collection_specific_id);
  if (it == id_to_storage_id.end()) {
    return false;
  }
  on_collection_association_.Run(it->second, collection);
  associated_nodes_.insert(handle);
  return true;
}

bool RestoreEntityTrackerAndroid::AssociatePinnedCollection(
    const PinnedTabCollection* collection) {
  return AssociateUniqueCollection(pinned_collection_id_, collection);
}

bool RestoreEntityTrackerAndroid::AssociateUnpinnedCollection(
    const UnpinnedTabCollection* collection) {
  return AssociateUniqueCollection(unpinned_collection_id_, collection);
}

bool RestoreEntityTrackerAndroid::AssociateTabStripCollection(
    const TabStripCollection* collection) {
  return AssociateUniqueCollection(tab_strip_collection_id_, collection);
}

bool RestoreEntityTrackerAndroid::AssociateTabGroupTabCollection(
    const TabGroupTabCollection* collection) {
  return AssociateCollectionUsingId(tab_group_id_to_storage_id_,
                                    collection->GetTabGroupId().token(),
                                    collection);
}

bool RestoreEntityTrackerAndroid::AssociateSplitTabCollection(
    const SplitTabCollection* collection) {
  return AssociateCollectionUsingId(split_tab_id_to_storage_id_,
                                    collection->GetSplitTabId().token(),
                                    collection);
}

bool RestoreEntityTrackerAndroid::HasCollectionBeenAssociated(
    TabCollection::Handle handle) {
  DCHECK(context_);
  if (context_->HasError()) {
    return false;
  }
  return associated_nodes_.contains(handle);
}

bool RestoreEntityTrackerAndroid::HasNothingToAssociate() {
  // Tab strip collection is the root collection, so if it does not need to be
  // associated, none of the other collections do either.
  return !tab_strip_collection_id_.has_value();
}

std::optional<StorageId> RestoreEntityTrackerAndroid::GetStorageIdForTab(
    int tab_android_id) {
  auto it = tab_android_id_to_storage_id_.find(tab_android_id);
  if (it == tab_android_id_to_storage_id_.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace tabs
