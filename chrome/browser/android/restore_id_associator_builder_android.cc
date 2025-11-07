// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/restore_id_associator_builder_android.h"

#include "chrome/browser/android/restore_id_associator_android.h"
#include "chrome/browser/tab/tab_storage_type.h"

namespace tabs {

RestoreIdAssociatorBuilderAndroid::RestoreIdAssociatorBuilderAndroid(
    OnTabAssociation on_tab_association,
    OnCollectionAssociation on_collection_association) {
  state_ = std::make_unique<RestoreIdAssociatorState>(
      on_tab_association, on_collection_association);
}

RestoreIdAssociatorBuilderAndroid::~RestoreIdAssociatorBuilderAndroid() =
    default;

void RestoreIdAssociatorBuilderAndroid::RegisterCollection(
    int storage_id,
    TabStorageType type,
    const tabs_pb::Children& children) {
  DCHECK(state_);
  state_->id_to_parent_id.reserve(children.storage_id_size());

  // Build a mapping of children IDs to parent IDs;
  for (int child_id : children.storage_id()) {
    state_->id_to_parent_id[child_id] = storage_id;
  }

  if (type == TabStorageType::kPinned) {
    DCHECK(!state_->pinned_collection_id)
        << "Should only have one pinned collection.";
    state_->pinned_collection_id = storage_id;
  }
}

void RestoreIdAssociatorBuilderAndroid::RegisterTab(
    int storage_id,
    const tabs_pb::TabState& tab_state) {
  DCHECK(state_);
  state_->tab_android_id_to_storage_id[tab_state.tab_id()] = storage_id;
}

std::unique_ptr<RestoreIdAssociator>
RestoreIdAssociatorBuilderAndroid::BuildAssociator() {
  DCHECK(state_);
  return std::make_unique<RestoreIdAssociatorAndroid>(std::move(state_));
}

}  // namespace tabs
