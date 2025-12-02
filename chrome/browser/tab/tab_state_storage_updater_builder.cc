// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_updater_builder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/storage_pending_updates.h"
#include "chrome/browser/tab/storage_update_unit.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_state_storage_updater.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "components/tabs/public/tab_collection.h"

namespace tabs {

TabStateStorageUpdaterBuilder::TabStateStorageUpdaterBuilder(
    StorageIdMapping& mapping,
    TabStoragePackager* packager)
    : mapping_(mapping),
      packager_(packager),
      updater_(std::make_unique<TabStateStorageUpdater>()) {}

TabStateStorageUpdaterBuilder::~TabStateStorageUpdaterBuilder() = default;

void TabStateStorageUpdaterBuilder::SaveNode(StorageId id,
                                             std::string window_tag,
                                             bool is_off_the_record,
                                             TabStorageType type,
                                             TabCollectionNodeHandle handle) {
  SaveNodePendingUpdate request(id, std::move(window_tag), is_off_the_record,
                                type, packager_, mapping_.get(),
                                std::move(handle));
  updater_->Add(request.CreateUnit());
}

void TabStateStorageUpdaterBuilder::SaveNodePayload(
    StorageId id,
    TabCollectionNodeHandle handle) {
  SavePayloadPendingUpdate request(id, packager_, mapping_.get(),
                                   std::move(handle));
  updater_->Add(request.CreateUnit());
}

void TabStateStorageUpdaterBuilder::SaveChildren(StorageId id,
                                                 TabCollectionHandle handle) {
  SaveChildrenPendingUpdate request(id, packager_, mapping_.get(),
                                    std::move(handle));
  updater_->Add(request.CreateUnit());
}

void TabStateStorageUpdaterBuilder::RemoveNode(StorageId id) {
  RemoveNodePendingUpdate request(id);
  updater_->Add(request.CreateUnit());
}

std::unique_ptr<TabStateStorageUpdater> TabStateStorageUpdaterBuilder::Build() {
  return std::move(updater_);
}

}  // namespace tabs
