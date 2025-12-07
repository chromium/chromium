// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_loaded_data.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/protocol/tab_strip_collection_state.pb.h"
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace tabs {

namespace {

// Strip + Pinned/Unpinned + TabGroup + Split + Tab = 5 levels.
constexpr int kMaxTreeHeight = 5;

// Performs a recursive in-order traversal of the tree rooted at
// `current_node_storage_id` in `children_map`. Tabs are drained from
// `loaded_tabs_map` and added to `sorted_tabs` in the order they are
// encountered during the traversal. The height of the tree is small
// so there is no risk of stack overflow from recursing.
void SortTabsInOrder(
    StorageId current_node_storage_id,
    std::optional<StorageId> active_tab_storage_id,
    const absl::flat_hash_map<StorageId, std::vector<StorageId>>& children_map,
    absl::flat_hash_map<StorageId, tabs_pb::TabState>& loaded_tabs_map,
    std::vector<tabs_pb::TabState>& sorted_tabs,
    std::optional<int>& active_tab_index,
    int depth = 0) {
  DCHECK_LE(depth, kMaxTreeHeight) << "Tree is too tall, possible cycle?";
  const auto it = children_map.find(current_node_storage_id);
  if (it != children_map.end()) {
    for (const auto& child_id : it->second) {
      SortTabsInOrder(child_id, active_tab_storage_id, children_map,
                      loaded_tabs_map, sorted_tabs, active_tab_index,
                      depth + 1);
    }
  } else {
    auto tab_it = loaded_tabs_map.find(current_node_storage_id);
    // TODO(crbug.com/460490530): DCHECK that we found a tab otherwise we have
    // collections that are missing their child tabs. Also consider emitting
    // a metric for this.
    if (tab_it != loaded_tabs_map.end()) {
      if (active_tab_storage_id.has_value() &&
          active_tab_storage_id.value() == current_node_storage_id) {
        active_tab_index = sorted_tabs.size();
      }
      sorted_tabs.emplace_back(std::move(tab_it->second));
      loaded_tabs_map.erase(tab_it);
    }
  }
}

}  // namespace

StorageLoadedData::Builder::Builder(
    std::unique_ptr<RestoreEntityTracker> tracker)
    : tracker_(std::move(tracker)) {}

StorageLoadedData::Builder::~Builder() = default;

StorageLoadedData::Builder::Builder(Builder&&) = default;
StorageLoadedData::Builder& StorageLoadedData::Builder::operator=(Builder&&) =
    default;

void StorageLoadedData::Builder::AddNode(
    StorageId id,
    TabStorageType type,
    base::span<const uint8_t> payload,
    base::PassKey<TabStateStorageDatabase> passkey) {
  if (type == TabStorageType::kTab) {
    tabs_pb::TabState tab_state;
    if (tab_state.ParseFromArray(payload.data(), payload.size())) {
      tracker_->RegisterTab(id, tab_state, passkey);
      loaded_tabs_map_.emplace(id, std::move(tab_state));
    } else {
      DLOG(ERROR) << "Failed to parse tab state for id: " << id;
    }
  } else if (type == TabStorageType::kTabStrip) {
    DCHECK(!root_storage_id_.has_value())
        << "Multiple root nodes for window tag in the database.";
    root_storage_id_ = id;
    tabs_pb::TabStripCollectionState tab_strip_state;
    if (tab_strip_state.ParseFromArray(payload.data(), payload.size())) {
      if (tab_strip_state.has_active_tab_storage_id()) {
        active_tab_storage_id_ =
            StorageIdFromTokenProto(tab_strip_state.active_tab_storage_id());
      }
    } else {
      DLOG(ERROR) << "Failed to parse tab strip state for id: " << id;
    }
  } else if (type == TabStorageType::kGroup) {
    tabs_pb::TabGroupCollectionState group_state;
    if (group_state.ParseFromArray(payload.data(), payload.size())) {
      loaded_groups_.emplace_back(
          std::make_unique<TabGroupCollectionData>(group_state));
    } else {
      DLOG(ERROR) << "Failed to parse group state for id: " << id;
    }
  }
}

void StorageLoadedData::Builder::AddChildren(
    StorageId id,
    TabStorageType type,
    base::span<const uint8_t> children,
    base::PassKey<TabStateStorageDatabase> passkey) {
  if (type == TabStorageType::kTab) {
    return;
  }
  tabs_pb::Children children_proto;
  if (children_proto.ParseFromArray(children.data(), children.size())) {
    tracker_->RegisterCollection(id, type, children_proto, passkey);
    std::vector<StorageId> storage_ids_vector;
    storage_ids_vector.reserve(children_proto.storage_id_size());
    for (const auto& child_id : children_proto.storage_id()) {
      storage_ids_vector.push_back(StorageIdFromTokenProto(child_id));
    }
    children_map_.emplace(id, std::move(storage_ids_vector));
  } else {
    DLOG(ERROR) << "Failed to parse children for id: " << id;
  }
}

std::unique_ptr<StorageLoadedData> StorageLoadedData::Builder::Build() {
  std::vector<tabs_pb::TabState> loaded_tabs;
  loaded_tabs.reserve(loaded_tabs_map_.size());
  std::optional<int> active_tab_index;

  // TODO(crbug.com/460490530): Change this to a DCHECK once we've worked out
  // why this is sometimes not present.
  if (root_storage_id_.has_value()) {
    SortTabsInOrder(root_storage_id_.value(), active_tab_storage_id_,
                    children_map_, loaded_tabs_map_, loaded_tabs,
                    active_tab_index);
  } else {
    // Temporarily fallback to just loading the tabs in a random order. It is
    // not possible to determine the `active_tab_index` as
    // `active_tab_storage_id` is not set if `root_storage_id` is not set.
    for (auto& [storage_id, tab] : loaded_tabs_map_) {
      loaded_tabs.emplace_back(std::move(tab));
    }
  }

  // TODO(crbug.com/460490530): CHECK that every tab row was found in the
  // child traversal. Otherwise we've got an inconsistent state and cleanup
  // may be necessary.

  StorageLoadedData* result =
      new StorageLoadedData(std::move(loaded_tabs), std::move(loaded_groups_),
                            std::move(tracker_), active_tab_index);
  return base::WrapUnique(result);
}

StorageLoadedData::StorageLoadedData(
    std::vector<tabs_pb::TabState> loaded_tabs,
    std::vector<std::unique_ptr<TabGroupCollectionData>> loaded_groups,
    std::unique_ptr<RestoreEntityTracker> tracker,
    std::optional<int> active_tab_index)
    : loaded_tabs_(std::move(loaded_tabs)),
      loaded_groups_(std::move(loaded_groups)),
      tracker_(std::move(tracker)),
      active_tab_index_(active_tab_index) {}
StorageLoadedData::~StorageLoadedData() = default;

StorageLoadedData::StorageLoadedData(StorageLoadedData&&) = default;
StorageLoadedData& StorageLoadedData::operator=(StorageLoadedData&&) = default;

RestoreEntityTracker* StorageLoadedData::GetTracker() const {
  return tracker_.get();
}

std::vector<tabs_pb::TabState>& StorageLoadedData::GetLoadedTabs() {
  return loaded_tabs_;
}

std::vector<std::unique_ptr<TabGroupCollectionData>>&
StorageLoadedData::GetLoadedGroups() {
  return loaded_groups_;
}

std::optional<int> StorageLoadedData::GetActiveTabIndex() const {
  return active_tab_index_;
}

}  // namespace tabs
