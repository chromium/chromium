// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_updater_builder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/storage_update_unit.h"
#include "chrome/browser/tab/storage_update_units.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_state_storage_updater.h"
#include "chrome/browser/tab/tab_storage_package.h"

namespace tabs {

TabStateStorageUpdaterBuilder::TabStateStorageUpdaterBuilder()
    : updater_(std::make_unique<TabStateStorageUpdater>()) {}

TabStateStorageUpdaterBuilder::~TabStateStorageUpdaterBuilder() = default;

void TabStateStorageUpdaterBuilder::SaveNode(
    StorageId id,
    std::string window_tag,
    bool is_off_the_record,
    TabStorageType type,
    std::unique_ptr<StoragePackage> package) {
  updater_->Add(std::make_unique<SaveNodeUpdateUnit>(
      id, std::move(window_tag), is_off_the_record, type, std::move(package)));
}

void TabStateStorageUpdaterBuilder::SaveNodePayload(
    StorageId id,
    std::unique_ptr<Payload> payload) {
  updater_->Add(
      std::make_unique<SavePayloadUpdateUnit>(id, std::move(payload)));
}

void TabStateStorageUpdaterBuilder::SaveChildren(
    StorageId id,
    std::unique_ptr<Payload> children) {
  updater_->Add(
      std::make_unique<SaveChildrenUpdateUnit>(id, std::move(children)));
}

void TabStateStorageUpdaterBuilder::RemoveNode(StorageId id) {
  updater_->Add(std::make_unique<RemoveNodeUpdateUnit>(id));
}

std::unique_ptr<TabStateStorageUpdater> TabStateStorageUpdaterBuilder::Build() {
  return std::move(updater_);
}

}  // namespace tabs
