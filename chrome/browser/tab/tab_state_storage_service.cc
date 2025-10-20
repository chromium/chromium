// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_service.h"

#include <memory>

#include "base/token.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_state_storage_updater_builder.h"
#include "chrome/browser/tab/tab_storage_packager.h"
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

}  // namespace

TabStateStorageService::TabStateStorageService(
    std::unique_ptr<TabStateStorageBackend> tab_backend,
    std::unique_ptr<TabStoragePackager> packager)
    : tab_backend_(std::move(tab_backend)), packager_(std::move(packager)) {
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
  if (!packager_) {
    return;
  }

  std::unique_ptr<StoragePackage> package = packager_->Package(tab);
  DCHECK(package) << "Packager should return a package";

  int storage_id = GetStorageId(tab);
  TabStateStorageUpdaterBuilder builder;
  builder.SaveNode(storage_id, TabStorageType::kTab, std::move(package));
  tab_backend_->Update(builder.Build());
}

void TabStateStorageService::Save(const TabCollection* collection) {
  if (!packager_) {
    return;
  }

  std::unique_ptr<StoragePackage> package =
      packager_->Package(collection, *this);
  DCHECK(package) << "Packager should return a package";

  int storage_id = GetStorageId(collection);
  TabStorageType type = TabCollectionTypeToTabStorageType(collection->type());
  TabStateStorageUpdaterBuilder builder;
  builder.SaveNode(storage_id, type, std::move(package));
  tab_backend_->Update(builder.Build());
}

void TabStateStorageService::LoadAllTabs(LoadAllTabsCallback callback) {
  tab_backend_->LoadAllNodes(
      base::BindOnce(&TabStateStorageService::OnAllTabsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TabStateStorageService::OnAllTabsLoaded(LoadAllTabsCallback callback,
                                             std::vector<NodeState> entries) {
  std::vector<LoadedTabState> loaded_tabs;
  int max_storage_id = 0;
  for (auto& entry : entries) {
    max_storage_id = std::max(max_storage_id, entry.id);
    if (entry.type == TabStorageType::kTab) {
      tabs_pb::TabState tab_state;
      if (tab_state.ParseFromString(entry.payload)) {
        loaded_tabs.emplace_back(
            std::move(tab_state),
            base::BindOnce(&TabStateStorageService::OnTabCreated,
                           weak_ptr_factory_.GetWeakPtr(), entry.id));
      }
    }
  }
  next_storage_id_ = max_storage_id + 1;
  std::move(callback).Run(std::move(loaded_tabs));
}

void TabStateStorageService::OnTabCreated(int storage_id,
                                          const TabInterface* tab) {
  if (tab == nullptr) {
    // TODO(https://crbug.com/448151790): Consider removing from the database.
    // Though if a complete post-initialization raze is coming, maybe it
    // doesn't matter.
    return;
  }

  tab_handle_to_storage_id_[tab->GetHandle().raw_value()] = storage_id;
}

}  // namespace tabs
