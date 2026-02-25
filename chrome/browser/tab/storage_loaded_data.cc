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
#include "base/notimplemented.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/protocol/tab_strip_collection_state.pb.h"
#include "chrome/browser/tab/restore_entity_tracker.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
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
    StorageLoadedData::StorageLoadingContext* context,
    int depth = 0) {
  if (depth > kMaxTreeHeight) {
    context->SetStatus(StorageLoadingStatus::kTreeTooDeepError,
                       "Tree is too tall, possible cycle?");
    return;
  }
  const auto it = children_map.find(current_node_storage_id);
  if (it != children_map.end()) {
    for (const auto& child_id : it->second) {
      if (context->HasError()) {
        return;
      }
      SortTabsInOrder(child_id, active_tab_storage_id, children_map,
                      loaded_tabs_map, sorted_tabs, active_tab_index, context,
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

StorageLoadedData::StorageLoadingContext::StorageLoadingContext() = default;
StorageLoadedData::StorageLoadingContext::~StorageLoadingContext() = default;

StorageLoadedData::StorageLoadingContext::StorageLoadingContext(
    StorageLoadingContext&&) = default;
StorageLoadedData::StorageLoadingContext&
StorageLoadedData::StorageLoadingContext::operator=(StorageLoadingContext&&) =
    default;

void StorageLoadedData::StorageLoadingContext::SetStatus(
    StorageLoadingStatus status,
    std::string message) {
  if (status_ == StorageLoadingStatus::kSuccess) {
    status_ = status;
    error_message_ = std::move(message);
  }
}

bool StorageLoadedData::StorageLoadingContext::HasError() const {
  return status_ != StorageLoadingStatus::kSuccess;
}

StorageLoadingStatus StorageLoadedData::StorageLoadingContext::status() const {
  return status_;
}

const std::optional<std::string>&
StorageLoadedData::StorageLoadingContext::error_message() const {
  return error_message_;
}

StorageLoadedData::Builder::Builder(
    std::string_view window_tag,
    bool is_off_the_record,
    std::unique_ptr<RestoreEntityTracker> tracker)
    : window_tag_(window_tag),
      is_off_the_record_(is_off_the_record),
      tracker_(std::move(tracker)) {
  tracker_->SetLoadingContext(&context_);
}

StorageLoadedData::Builder::~Builder() = default;

StorageLoadedData::Builder::Builder(Builder&&) = default;
StorageLoadedData::Builder& StorageLoadedData::Builder::operator=(Builder&&) =
    default;

void StorageLoadedData::Builder::AddNode(
    StorageId id,
    TabStorageType type,
    base::span<const uint8_t> payload,
    std::optional<base::span<const uint8_t>> children,
    base::PassKey<TabStateStorageDatabase> passkey) {
  if (context_.HasError()) {
    return;
  }

  if (type == TabStorageType::kTab) {
    tabs_pb::TabState tab_state;
    if (tab_state.ParseFromArray(payload.data(), payload.size())) {
      tracker_->RegisterTab(id, tab_state, passkey);
      loaded_tabs_map_.emplace(id, std::move(tab_state));
    } else {
      context_.SetStatus(StorageLoadingStatus::kParseError,
                         "Failed to parse tab state for id: " + id.ToString());
    }
    return;
  }

  DCHECK(children.has_value());
  std::optional<tabs_pb::Children> children_proto =
      ParseChildren(id, *children, children_map_);
  if (!children_proto.has_value()) {
    return;
  }

  std::optional<base::Token> collection_specific_id;
  if (type == TabStorageType::kTabStrip) {
    if (root_storage_id_.has_value()) {
      context_.SetStatus(StorageLoadingStatus::kMultipleUniqueNodesError,
                         "Multiple root nodes for window tag in the database.");
      return;
    }
    root_storage_id_ = id;
    tabs_pb::TabStripCollectionState tab_strip_state;
    if (tab_strip_state.ParseFromArray(payload.data(), payload.size())) {
      if (tab_strip_state.has_active_tab_storage_id()) {
        active_tab_storage_id_ =
            StorageIdFromTokenProto(tab_strip_state.active_tab_storage_id());
      }
    } else {
      context_.SetStatus(
          StorageLoadingStatus::kParseError,
          "Failed to parse tab strip state for id: " + id.ToString());
    }
  } else if (type == TabStorageType::kGroup) {
    tabs_pb::TabGroupCollectionState group_state;
    if (group_state.ParseFromArray(payload.data(), payload.size())) {
      loaded_groups_.emplace_back(
          std::make_unique<TabGroupCollectionData>(group_state));
      collection_specific_id = loaded_groups_.back()->tab_group_id_;
    } else {
      context_.SetStatus(
          StorageLoadingStatus::kParseError,
          "Failed to parse group state for id: " + id.ToString());
    }
  } else if (type == TabStorageType::kSplit) {
    // TODO(crbug.com/485306544): Add support for split collections once they
    // are supported on Android.
    NOTIMPLEMENTED();
  }
  tracker_->RegisterCollection(id, type, *children_proto, collection_specific_id,
                               passkey);
}

void StorageLoadedData::Builder::AddDivergentNode(
    StorageId id,
    TabStorageType type,
    std::optional<base::span<const uint8_t>> children,
    base::PassKey<TabStateStorageDatabase> passkey) {
  if (context_.HasError()) {
    return;
  }

  DCHECK(children.has_value());
  std::optional<tabs_pb::Children> children_proto =
      ParseChildren(id, *children, divergent_children_map_);
  if (!children_proto.has_value()) {
    return;
  }
}

void StorageLoadedData::Builder::ReconcileDivergentNodes(
    base::PassKey<Builder> builder_passkey,
    TabStateStorageDatabase* database) {
  if (divergent_children_map_.empty()) {
    return;
  }

  // Collect all children that are part of any divergent vector.
  absl::flat_hash_set<StorageId> all_divergent_children;
  for (const auto& [id, children] : divergent_children_map_) {
    for (const auto& child_id : children) {
      all_divergent_children.insert(child_id);
    }
  }

  absl::flat_hash_set<StorageId> deleted_nodes = BuildDeletedNodesSet();

  TabStateStorageDatabase::OpenTransaction* transaction =
      database->CreateTransaction();
  for (auto& [id, divergent_children] : divergent_children_map_) {
    std::vector<StorageId> reconciled_children = std::move(divergent_children);

    auto canonical_it = children_map_.find(id);
    if (canonical_it != children_map_.end()) {
      bool found_divergent_match = false;

      // Append canonical children starting from the first that is not in any
      // divergent vector and not deleted.
      for (const auto& canonical_child : canonical_it->second) {
        if (deleted_nodes.contains(canonical_child)) {
          continue;
        } else if (found_divergent_match) {
          reconciled_children.push_back(canonical_child);
        } else if (all_divergent_children.find(canonical_child) ==
                       all_divergent_children.end()) {
          found_divergent_match = true;
        }
      }
    }

    // The reconciled children vector is now canonical. Save it to the database.
    tabs_pb::Children children_proto;
    for (const auto& child_id : reconciled_children) {
      tabs_pb::Token* token = children_proto.add_storage_id();
      StorageIdToTokenProto(child_id, token);
    }

    std::vector<uint8_t> serialized_children(children_proto.ByteSizeLong());
    children_proto.SerializeToArray(serialized_children.data(),
                                    serialized_children.size());
    database->SaveNodeChildren(transaction, id, std::move(serialized_children));

    children_map_[id] = std::move(reconciled_children);
  }

  // Clear the divergent window from the database now that we have reconciled
  // the divergent data.
  database->ClearDivergentNodesForWindow(window_tag_, is_off_the_record_);

  database->CloseTransaction(transaction);
}

absl::flat_hash_set<StorageId>
StorageLoadedData::Builder::BuildDeletedNodesSet() {
  absl::flat_hash_set<StorageId> deleted_nodes;
  for (const auto& [_, children] : children_map_) {
    for (const auto& child_id : children) {
      if (!children_map_.contains(child_id) &&
          !loaded_tabs_map_.contains(child_id)) {
        deleted_nodes.insert(child_id);
      }
    }
  }
  return deleted_nodes;
}

std::optional<tabs_pb::Children> StorageLoadedData::Builder::ParseChildren(
    StorageId id,
    base::span<const uint8_t> children_payload,
    absl::flat_hash_map<StorageId, std::vector<StorageId>>& children_map) {
  tabs_pb::Children children_proto;
  if (!children_proto.ParseFromArray(children_payload.data(),
                                     children_payload.size())) {
    context_.SetStatus(StorageLoadingStatus::kParseError,
                       "Failed to parse children for id: " + id.ToString());
    return std::nullopt;
  }

  std::vector<StorageId> storage_ids_vector;
  storage_ids_vector.reserve(children_proto.storage_id_size());
  for (const tabs_pb::Token& child_id : children_proto.storage_id()) {
    storage_ids_vector.push_back(StorageIdFromTokenProto(child_id));
  }
  children_map.emplace(id, std::move(storage_ids_vector));

  return std::move(children_proto);
}

std::unique_ptr<StorageLoadedData> StorageLoadedData::Builder::Build(
    base::PassKey<TabStateStorageDatabase> passkey,
    TabStateStorageDatabase* database) {
  if (context_.HasError()) {
    return base::WrapUnique(new StorageLoadedData(
        window_tag_, is_off_the_record_, std::vector<tabs_pb::TabState>(),
        std::vector<std::unique_ptr<TabGroupCollectionData>>(),
        std::move(tracker_), std::nullopt, std::move(context_)));
  }

  ReconcileDivergentNodes(base::PassKey<Builder>(), database);

  std::vector<tabs_pb::TabState> loaded_tabs;
  loaded_tabs.reserve(loaded_tabs_map_.size());
  std::optional<int> active_tab_index;

  // TODO(crbug.com/460490530): Change this to a DCHECK once we've worked out
  // why this is sometimes not present.
  if (root_storage_id_.has_value()) {
    SortTabsInOrder(root_storage_id_.value(), active_tab_storage_id_,
                    children_map_, loaded_tabs_map_, loaded_tabs,
                    active_tab_index, &context_);
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

  StorageLoadedData* result = new StorageLoadedData(
      window_tag_, is_off_the_record_, std::move(loaded_tabs),
      std::move(loaded_groups_), std::move(tracker_), active_tab_index,
      std::move(context_));
  return base::WrapUnique(result);
}

StorageLoadedData::StorageLoadedData(
    std::string_view window_tag,
    bool is_off_the_record,
    std::vector<tabs_pb::TabState> loaded_tabs,
    std::vector<std::unique_ptr<TabGroupCollectionData>> loaded_groups,
    std::unique_ptr<RestoreEntityTracker> tracker,
    std::optional<int> active_tab_index,
    StorageLoadingContext context)
    : window_tag_(window_tag),
      is_off_the_record_(is_off_the_record),
      loaded_tabs_(std::move(loaded_tabs)),
      loaded_groups_(std::move(loaded_groups)),
      tracker_(std::move(tracker)),
      active_tab_index_(active_tab_index),
      context_(std::move(context)) {
  // Re-assign context, since it was moved.
  tracker_->SetLoadingContext(&context_);
}

StorageLoadedData::~StorageLoadedData() = default;

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

const StorageLoadedData::StorageLoadingContext&
StorageLoadedData::GetLoadingContext() const {
  return context_;
}

const std::string& StorageLoadedData::GetWindowTag() const {
  return window_tag_;
}

bool StorageLoadedData::IsOffTheRecord() const {
  return is_off_the_record_;
}

void StorageLoadedData::NotifyNodeRejected(StorageId node) {
  observers_.Notify(&Observer::OnNodeRejected, node);
}

void StorageLoadedData::RegisterObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void StorageLoadedData::UnregisterObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace tabs
