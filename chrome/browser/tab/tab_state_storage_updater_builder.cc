// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_updater_builder.h"

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "base/logging.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/storage_pending_updates.h"
#include "chrome/browser/tab/storage_update_unit.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "chrome/browser/tab/tab_storage_util.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

TabStateStorageUpdaterBuilder::TabStateStorageUpdaterBuilder(
    StorageIdMapping& mapping,
    TabStoragePackager* packager)
    : mapping_(mapping), packager_(packager) {}

TabStateStorageUpdaterBuilder::~TabStateStorageUpdaterBuilder() = default;

TabStateStorageUpdaterBuilder::TabStateStorageUpdaterBuilder(
    TabStateStorageUpdaterBuilder&&) = default;
TabStateStorageUpdaterBuilder& TabStateStorageUpdaterBuilder::operator=(
    TabStateStorageUpdaterBuilder&&) = default;

bool TabStateStorageUpdaterBuilder::ContainsUpdateWithAnyType(
    StorageId id,
    std::initializer_list<UnitType> types) {
  auto it = update_for_id_.find(id);
  if (it == update_for_id_.end()) {
    return false;
  }

  const UnitType update_type = it->second->type();
  return std::ranges::contains(types, update_type);
}

void TabStateStorageUpdaterBuilder::SquashIntoSaveNode(
    StorageId id,
    const TabCollection* collection) {
  update_for_id_[id] = std::make_unique<SaveNodePendingUpdate>(
      id, packager_->GetWindowTag(collection),
      packager_->IsOffTheRecord(collection),
      TabCollectionTypeToTabStorageType(collection->type()), packager_,
      mapping_.get(), collection->GetHandle());
}

void TabStateStorageUpdaterBuilder::SaveNode(StorageId id,
                                             std::string window_tag,
                                             bool is_off_the_record,
                                             TabStorageType type,
                                             TabCollectionNodeHandle handle) {
  if (ContainsUpdateWithAnyType(id, {UnitType::kSaveNode})) {
    return;
  }

  update_for_id_[id] = std::make_unique<SaveNodePendingUpdate>(
      id, std::move(window_tag), is_off_the_record, type, packager_,
      mapping_.get(), handle);
}

void TabStateStorageUpdaterBuilder::SaveNodePayload(
    StorageId id,
    TabCollectionNodeHandle handle) {
  if (ContainsUpdateWithAnyType(
          id, {UnitType::kSaveNode, UnitType::kSavePayload})) {
    return;
  }

  if (ContainsUpdateWithAnyType(id, {UnitType::kSaveChildren})) {
    DCHECK(std::holds_alternative<TabCollectionHandle>(handle))
        << "Tabs do not have children";
    SquashIntoSaveNode(id, std::get<TabCollectionHandle>(handle).Get());
    return;
  }

  const TabCollection* collection = nullptr;
  if (std::holds_alternative<TabHandle>(handle)) {
    collection = std::get<TabHandle>(handle).Get()->GetParentCollection();
    DCHECK(collection) << "Tab must have a parent collection to be saved.";
  } else {
    collection = std::get<TabCollectionHandle>(handle).Get();
    DCHECK(collection) << "Collection must not be null.";
  }

  update_for_id_[id] = std::make_unique<SavePayloadPendingUpdate>(
      id, packager_->GetWindowTag(collection),
      packager_->IsOffTheRecord(collection), packager_, mapping_.get(), handle);
}

void TabStateStorageUpdaterBuilder::SaveChildren(
    StorageId id,
    const TabCollection* collection) {
  if (ContainsUpdateWithAnyType(
          id, {UnitType::kSaveNode, UnitType::kSaveChildren})) {
    return;
  }

  if (ContainsUpdateWithAnyType(id, {UnitType::kSavePayload})) {
    SquashIntoSaveNode(id, collection);
    return;
  }

  update_for_id_[id] = std::make_unique<SaveChildrenPendingUpdate>(
      id, packager_, mapping_.get(), collection->GetHandle());
}

void TabStateStorageUpdaterBuilder::SaveDivergentChildren(
    StorageId id,
    const TabCollection* collection) {
  auto [it, inserted] = divergence_update_for_id_.try_emplace(id);
  if (!inserted) {
    return;
  }
  it->second = std::make_unique<SaveDivergentChildrenPendingUpdate>(
      id, packager_->GetWindowTag(collection),
      packager_->IsOffTheRecord(collection), packager_, mapping_.get(),
      collection->GetHandle());
}

void TabStateStorageUpdaterBuilder::RemoveNode(StorageId id) {
  if (ContainsUpdateWithAnyType(id, {UnitType::kRemoveNode})) {
    return;
  }

  update_for_id_[id] = std::make_unique<RemoveNodePendingUpdate>(id);
}

std::unique_ptr<TabStateStorageUpdater> TabStateStorageUpdaterBuilder::Build() {
  auto updater = std::make_unique<TabStateStorageUpdater>();
  for (auto& [id, update] : update_for_id_) {
    updater->Add(update->CreateUnit());
  }
  for (auto& [id, update] : divergence_update_for_id_) {
    updater->Add(update->CreateUnit());
  }
  return updater;
}

}  // namespace tabs
