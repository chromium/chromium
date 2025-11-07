// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_service.h"

#include <memory>
#include <utility>

#include "base/token.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/restore_id_associator.h"
#include "chrome/browser/tab/restore_id_associator_builder.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "chrome/browser/tab/tab_state_storage_updater_builder.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "chrome/browser/tab/tab_storage_util.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

namespace {

template <typename T>
int GetOrCreateStorageId(T* object,
                         absl::flat_hash_map<int32_t, int>& handle_map,
                         int& next_storage_id) {
  int32_t handle_id = object->GetHandle().raw_value();
  auto [it, inserted] = handle_map.try_emplace(handle_id, next_storage_id);
  if (inserted) {
    next_storage_id++;
  }
  return it->second;
}

// Adds a save children operation to the builder.
void SaveChildrenInternal(TabStateStorageUpdaterBuilder& builder,
                          const TabCollection* parent,
                          TabStateStorageService* service,
                          TabStoragePackager* packager) {
  builder.SaveChildren(service->GetStorageId(parent),
                       packager->PackageChildren(parent, *service));
}

void RemoveNodeSequence(int storage_id,
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

}  // namespace

TabStateStorageService::TabStateStorageService(
    std::unique_ptr<TabStateStorageBackend> tab_backend,
    std::unique_ptr<TabStoragePackager> packager,
    TabCanonicalizer tab_canonicalizer,
    AssociatorBuilderFactory builder_factory)
    : tab_backend_(std::move(tab_backend)),
      packager_(std::move(packager)),
      tab_canonicalizer_(tab_canonicalizer),
      builder_factory_(builder_factory) {
  tab_backend_->Initialize();
}

TabStateStorageService::~TabStateStorageService() = default;

int TabStateStorageService::GetStorageId(const TabCollection* collection) {
  return ::tabs::GetOrCreateStorageId(
      collection, collection_handle_to_storage_id_, next_storage_id_);
}

int TabStateStorageService::GetStorageId(const TabInterface* tab) {
  return ::tabs::GetOrCreateStorageId(tab, tab_handle_to_storage_id_,
                                      next_storage_id_);
}

void TabStateStorageService::Save(const TabInterface* tab) {
  DCHECK(packager_);

  std::unique_ptr<StoragePackage> package = packager_->Package(tab);
  DCHECK(package) << "Packager should return a package";

  int storage_id = GetStorageId(tab);
  TabStateStorageUpdaterBuilder builder;
  builder.SaveNode(storage_id, TabStorageType::kTab, std::move(package));
  tab_backend_->Update(builder.Build());
}

void TabStateStorageService::Save(const TabCollection* collection) {
  DCHECK(packager_);

  std::unique_ptr<StoragePackage> package =
      packager_->Package(collection, *this);
  DCHECK(package) << "Packager should return a package";

  int storage_id = GetStorageId(collection);
  TabStorageType type = TabCollectionTypeToTabStorageType(collection->type());
  TabStateStorageUpdaterBuilder builder;
  builder.SaveNode(storage_id, type, std::move(package));
  tab_backend_->Update(builder.Build());
}

void TabStateStorageService::SavePayload(const TabCollection* collection) {
  DCHECK(packager_);

  std::unique_ptr<Payload> payload =
      packager_->PackagePayload(collection, *this);
  DCHECK(payload) << "Packager should return a payload";

  int storage_id = GetStorageId(collection);
  TabStateStorageUpdaterBuilder builder;
  builder.SaveNodePayload(storage_id, std::move(payload));
  tab_backend_->Update(builder.Build());
}

void TabStateStorageService::Remove(const TabInterface* tab) {
  RemoveNodeSequence(GetStorageId(tab), tab->GetParentCollection(), this,
                     packager_.get(), tab_backend_.get());
}

void TabStateStorageService::Remove(const TabCollection* collection) {
  RemoveNodeSequence(GetStorageId(collection),
                     collection->GetParentCollection(), this, packager_.get(),
                     tab_backend_.get());
}

void TabStateStorageService::Move(const TabInterface* tab,
                                  const TabCollection* prev_parent) {
  MoveNodeSequence(prev_parent, tab->GetParentCollection(), this,
                   packager_.get(), tab_backend_.get());
}

void TabStateStorageService::Move(const TabCollection* collection,
                                  const TabCollection* prev_parent) {
  MoveNodeSequence(prev_parent, collection->GetParentCollection(), this,
                   packager_.get(), tab_backend_.get());
}

void TabStateStorageService::LoadAllNodes(LoadDataCallback callback) {
  tab_backend_->LoadAllNodes(
      base::BindOnce(&TabStateStorageService::OnAllNodesLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TabStateStorageService::ClearState() {
  tab_backend_->ClearAllNodes();
}

void TabStateStorageService::OnAllNodesLoaded(LoadDataCallback callback,
                                              std::vector<NodeState> entries) {
  auto on_tab_association = base::BindRepeating(
      &TabStateStorageService::OnTabCreated, weak_ptr_factory_.GetWeakPtr());
  auto on_collection_association =
      base::BindRepeating(&TabStateStorageService::OnCollectionCreated,
                          weak_ptr_factory_.GetWeakPtr());

  std::vector<LoadedTabState> loaded_tabs;
  std::vector<std::unique_ptr<TabGroupCollectionData>> loaded_groups;
  std::unique_ptr<RestoreIdAssociatorBuilder> builder =
      builder_factory_.Run(on_tab_association, on_collection_association);

  DCHECK(builder) << "Associator builder has not been instantiated";

  int max_storage_id = 0;
  for (auto& entry : entries) {
    max_storage_id = std::max(max_storage_id, entry.id);
    if (entry.type == TabStorageType::kTab) {
      tabs_pb::TabState tab_state;
      if (tab_state.ParseFromString(entry.payload)) {
        builder->RegisterTab(entry.id, tab_state);
        loaded_tabs.emplace_back(
            std::move(tab_state),
            base::BindOnce(&TabStateStorageService::OnTabCreated,
                           weak_ptr_factory_.GetWeakPtr(), entry.id));
      }
    } else {
      tabs_pb::Children children;
      if (children.ParseFromString(entry.children)) {
        builder->RegisterCollection(entry.id, children);
      }

      if (entry.type == TabStorageType::kGroup) {
        tabs_pb::TabGroupCollectionState group_state;
        if (group_state.ParseFromString(entry.payload)) {
          loaded_groups.emplace_back(
              std::make_unique<TabGroupCollectionData>(group_state));
        }
      }
    }
  }
  next_storage_id_ = max_storage_id + 1;

  auto loaded_data = std::make_unique<StorageLoadedData>(
      std::move(loaded_tabs), std::move(loaded_groups),
      builder->BuildAssociator());
  std::move(callback).Run(std::move(loaded_data));
}

void TabStateStorageService::OnTabCreated(int storage_id,
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
    int storage_id,
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
