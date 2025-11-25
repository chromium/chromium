// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_service.h"

#include <memory>
#include <utility>

#include "base/token.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/protocol/tab_strip_collection_state.pb.h"
#include "chrome/browser/tab/protocol/token.pb.h"
#include "chrome/browser/tab/restore_id_associator.h"
#include "chrome/browser/tab/restore_id_associator_builder.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "chrome/browser/tab/tab_state_storage_updater_builder.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "chrome/browser/tab/tab_storage_util.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace tabs {

namespace {

template <typename T>
StorageId GetOrCreateStorageId(
    T* object,
    absl::flat_hash_map<int32_t, StorageId>& handle_map) {
  int32_t handle_id = object->GetHandle().raw_value();
  auto it = handle_map.find(handle_id);
  if (it != handle_map.end()) {
    return it->second;
  }
  StorageId storage_id = StorageId::Create();
  handle_map[handle_id] = storage_id;
  return storage_id;
}

// Adds a save children operation to the builder.
void SaveChildrenInternal(TabStateStorageUpdaterBuilder& builder,
                          const TabCollection* parent,
                          TabStateStorageService* service,
                          TabStoragePackager* packager) {
  builder.SaveChildren(service->GetStorageId(parent),
                       packager->PackageChildren(parent, *service));
}

void RemoveNodeSequence(StorageId storage_id,
                        const TabCollection* parent,
                        TabStateStorageService* service,
                        TabStoragePackager* packager,
                        TabStateStorageBackend* backend) {
  DCHECK(packager);

  TabStateStorageUpdaterBuilder builder;
  builder.RemoveNode(storage_id);

  SaveChildrenInternal(builder, parent, service, packager);
  backend->Update(builder.Build());
}

void MoveNodeSequence(const TabCollection* prev_parent,
                      const TabCollection* curr_parent,
                      TabStateStorageService* service,
                      TabStoragePackager* packager,
                      TabStateStorageBackend* backend) {
  DCHECK(packager);

  TabStateStorageUpdaterBuilder builder;
  SaveChildrenInternal(builder, prev_parent, service, packager);
  SaveChildrenInternal(builder, curr_parent, service, packager);
  backend->Update(builder.Build());
}

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

TabStateStorageService::TabStateStorageService(
    const base::FilePath& profile_path,
    std::unique_ptr<TabStoragePackager> packager,
    TabCanonicalizer tab_canonicalizer,
    AssociatorBuilderFactory builder_factory)
    : tab_backend_(profile_path),
      packager_(std::move(packager)),
      tab_canonicalizer_(tab_canonicalizer),
      builder_factory_(builder_factory) {
  tab_backend_.Initialize();
}

TabStateStorageService::~TabStateStorageService() = default;

StorageId TabStateStorageService::GetStorageId(
    const TabCollection* collection) {
  return ::tabs::GetOrCreateStorageId(collection,
                                      collection_handle_to_storage_id_);
}

StorageId TabStateStorageService::GetStorageId(const TabInterface* tab) {
  return ::tabs::GetOrCreateStorageId(tab_canonicalizer_.Run(tab),
                                      tab_handle_to_storage_id_);
}

void TabStateStorageService::Save(const TabInterface* tab) {
  DCHECK(packager_);

  std::unique_ptr<StoragePackage> package = packager_->Package(tab);
  DCHECK(package) << "Packager should return a package";

  const TabCollection* parent = tab->GetParentCollection();
  DCHECK(tab->GetParentCollection()) << "Tab must have a parent collection";
  std::string window_tag = packager_->GetWindowTag(parent);
  bool is_off_the_record = packager_->IsOffTheRecord(parent);

  StorageId storage_id = GetStorageId(tab);
  TabStateStorageUpdaterBuilder builder;
  builder.SaveNode(storage_id, std::move(window_tag), is_off_the_record,
                   TabStorageType::kTab, std::move(package));
  tab_backend_.Update(builder.Build());
}

void TabStateStorageService::Save(const TabCollection* collection) {
  DCHECK(packager_);

  std::unique_ptr<StoragePackage> package =
      packager_->Package(collection, *this);
  DCHECK(package) << "Packager should return a package";

  std::string window_tag = packager_->GetWindowTag(collection);
  bool is_off_the_record = packager_->IsOffTheRecord(collection);

  StorageId storage_id = GetStorageId(collection);
  TabStorageType type = TabCollectionTypeToTabStorageType(collection->type());
  TabStateStorageUpdaterBuilder builder;
  builder.SaveNode(storage_id, std::move(window_tag), is_off_the_record, type,
                   std::move(package));
  tab_backend_.Update(builder.Build());
}

void TabStateStorageService::SavePayload(const TabCollection* collection) {
  DCHECK(packager_);

  std::unique_ptr<Payload> payload =
      packager_->PackagePayload(collection, *this);
  DCHECK(payload) << "Packager should return a payload";

  StorageId storage_id = GetStorageId(collection);
  TabStateStorageUpdaterBuilder builder;
  builder.SaveNodePayload(storage_id, std::move(payload));
  tab_backend_.Update(builder.Build());
}

void TabStateStorageService::Remove(const TabInterface* tab,
                                    const TabCollection* prev_parent) {
  RemoveNodeSequence(GetStorageId(tab), prev_parent, this, packager_.get(),
                     &tab_backend_);
}

void TabStateStorageService::Remove(const TabCollection* collection,
                                    const TabCollection* prev_parent) {
  RemoveNodeSequence(GetStorageId(collection), prev_parent, this,
                     packager_.get(), &tab_backend_);
}

void TabStateStorageService::Move(const TabInterface* tab,
                                  const TabCollection* prev_parent) {
  MoveNodeSequence(prev_parent, tab->GetParentCollection(), this,
                   packager_.get(), &tab_backend_);
}

void TabStateStorageService::Move(const TabCollection* collection,
                                  const TabCollection* prev_parent) {
  MoveNodeSequence(prev_parent, collection->GetParentCollection(), this,
                   packager_.get(), &tab_backend_);
}

void TabStateStorageService::LoadAllNodes(std::string window_tag,
                                          bool is_off_the_record,
                                          LoadDataCallback callback) {
  tab_backend_.LoadAllNodes(
      std::move(window_tag), is_off_the_record,
      base::BindOnce(&TabStateStorageService::OnAllNodesLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TabStateStorageService::ClearState() {
  tab_backend_.ClearAllNodes();
}

void TabStateStorageService::OnAllNodesLoaded(LoadDataCallback callback,
                                              std::vector<NodeState> entries) {
  auto on_tab_association = base::BindRepeating(
      &TabStateStorageService::OnTabCreated, weak_ptr_factory_.GetWeakPtr());
  auto on_collection_association =
      base::BindRepeating(&TabStateStorageService::OnCollectionCreated,
                          weak_ptr_factory_.GetWeakPtr());

  std::unique_ptr<RestoreIdAssociatorBuilder> builder =
      builder_factory_.Run(on_tab_association, on_collection_association);

  DCHECK(builder) << "Associator builder has not been instantiated";

  if (entries.empty()) {
    std::move(callback).Run(std::make_unique<StorageLoadedData>(
        std::vector<tabs_pb::TabState>(),
        std::vector<std::unique_ptr<TabGroupCollectionData>>(),
        builder->BuildAssociator(), std::nullopt));
    return;
  }

  std::optional<StorageId> root_storage_id;
  std::optional<StorageId> active_tab_storage_id;
  absl::flat_hash_map<StorageId, tabs_pb::TabState> loaded_tabs_map;
  absl::flat_hash_map<StorageId, std::vector<StorageId>> children_map;
  std::vector<std::unique_ptr<TabGroupCollectionData>> loaded_groups;

  for (auto& entry : entries) {
    if (entry.type == TabStorageType::kTab) {
      tabs_pb::TabState tab_state;
      if (tab_state.ParseFromArray(entry.payload.data(),
                                   entry.payload.size())) {
        builder->RegisterTab(entry.id, tab_state);
        loaded_tabs_map.emplace(entry.id, std::move(tab_state));
      }
    } else {
      if (entry.type == TabStorageType::kTabStrip) {
        DCHECK(!root_storage_id.has_value())
            << "Multiple root nodes for window tag in the database.";
        root_storage_id = entry.id;
        tabs_pb::TabStripCollectionState tab_strip_state;
        if (tab_strip_state.ParseFromArray(entry.payload.data(),
                                           entry.payload.size())) {
          if (tab_strip_state.has_active_tab_storage_id()) {
            active_tab_storage_id = StorageIdFromTokenProto(
                tab_strip_state.active_tab_storage_id());
          }
        }
      }
      tabs_pb::Children children;
      if (children.ParseFromArray(entry.children.data(),
                                  entry.children.size())) {
        builder->RegisterCollection(entry.id, entry.type, children);
        std::vector<StorageId> storage_ids_vector;
        storage_ids_vector.reserve(children.storage_id_size());
        for (const auto& storage_id : children.storage_id()) {
          storage_ids_vector.push_back(StorageIdFromTokenProto(storage_id));
        }
        children_map.emplace(entry.id, std::move(storage_ids_vector));
      }

      if (entry.type == TabStorageType::kGroup) {
        tabs_pb::TabGroupCollectionState group_state;
        if (group_state.ParseFromArray(entry.payload.data(),
                                       entry.payload.size())) {
          loaded_groups.emplace_back(
              std::make_unique<TabGroupCollectionData>(group_state));
        }
      }
    }
  }

  std::vector<tabs_pb::TabState> loaded_tabs;
  loaded_tabs.reserve(loaded_tabs_map.size());
  std::optional<int> active_tab_index;

  // TODO(crbug.com/460490530): Change this to a DCHECK once we've worked out
  // why this is sometimes not present.
  if (root_storage_id.has_value()) {
    SortTabsInOrder(root_storage_id.value(), active_tab_storage_id,
                    children_map, loaded_tabs_map, loaded_tabs,
                    active_tab_index);
  } else {
    // Temporarily fallback to just loading the tabs in a random order. It is
    // not possible to determine the `active_tab_index` as
    // `active_tab_storage_id` is not set if `root_storage_id` is not set.
    for (auto& [storage_id, tab] : loaded_tabs_map) {
      loaded_tabs.emplace_back(std::move(tab));
    }
  }

  // TODO(crbug.com/460490530): CHECK that every tab row was found in the
  // child traversal. Otherwise we've got an inconsistent state and cleanup
  // may be necessary.

  auto loaded_data = std::make_unique<StorageLoadedData>(
      std::move(loaded_tabs), std::move(loaded_groups),
      builder->BuildAssociator(), active_tab_index);
  std::move(callback).Run(std::move(loaded_data));
}

void TabStateStorageService::OnTabCreated(StorageId storage_id,
                                          const TabInterface* tab) {
  const TabInterface* canonicalized_tab = tab_canonicalizer_.Run(tab);
  if (canonicalized_tab == nullptr) {
    // TODO(https://crbug.com/448151790): Consider removing from the database.
    // Though if a complete post-initialization raze is coming, maybe it
    // doesn't matter.
    return;
  }

  tab_handle_to_storage_id_[canonicalized_tab->GetHandle().raw_value()] =
      storage_id;
}

void TabStateStorageService::OnCollectionCreated(
    StorageId storage_id,
    const TabCollection* collection) {
  if (collection == nullptr) {
    // TODO(https://crbug.com/448151790): Consider removing from the database.
    // Though if a complete post-initialization raze is coming, maybe it
    // doesn't matter.
    return;
  }

  collection_handle_to_storage_id_[collection->GetHandle().raw_value()] =
      storage_id;
}

}  // namespace tabs
