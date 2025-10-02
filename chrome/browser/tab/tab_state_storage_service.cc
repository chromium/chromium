// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_service.h"

#include <memory>

#include "base/token.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

TabStateStorageService::TabStateStorageService(
    std::unique_ptr<TabStateStorageBackend> tab_backend,
    std::unique_ptr<TabStoragePackager> packager)
    : tab_backend_(std::move(tab_backend)), packager_(std::move(packager)) {
  tab_backend_->Initialize();
}

TabStateStorageService::~TabStateStorageService() = default;

void TabStateStorageService::SaveTab(const TabInterface* tab) {
  if (!packager_) {
    return;
  }

  packager_->Package(tab);
  std::unique_ptr<StoragePackage> package = packager_->ReleasePackage();
  DCHECK(package) << "Packager should return a package";
  // TODO(https://crbug.com/448875689): Save through backend when we have an id.
}

void TabStateStorageService::SaveCollection(
    const tabs::TabCollection* collection) {
  if (!packager_) {
    return;
  }

  packager_->Package(collection);
  std::unique_ptr<StoragePackage> package = packager_->ReleasePackage();
  DCHECK(package) << "Packager should return a package";
  // TODO(https://crbug.com/448875689): Save through backend when we have an id.
}

void TabStateStorageService::LoadAllTabs(LoadAllTabsCallback callback) {
  tab_backend_->LoadAllNodes(
      base::BindOnce(&TabStateStorageService::OnAllTabsLoaded,
                     base::Unretained(this), std::move(callback)));
}

void TabStateStorageService::OnAllTabsLoaded(LoadAllTabsCallback callback,
                                             std::vector<NodeState> entries) {
  std::vector<tabs_pb::TabState> tab_states;
  for (auto& entry : entries) {
    if (entry.type == 1) {
      tabs_pb::TabState tab_state;
      if (tab_state.ParseFromString(entry.payload)) {
        tab_states.emplace_back(std::move(tab_state));
      }
    }
  }
  std::move(callback).Run(std::move(tab_states));
}

}  // namespace tabs
