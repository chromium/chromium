// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_update_units.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/storage_update_unit.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_storage_package.h"

namespace tabs {

using OpenTransaction = TabStateStorageDatabase::OpenTransaction;

SaveNodeUpdateUnit::SaveNodeUpdateUnit(StorageId id,
                                       std::string window_tag,
                                       bool is_off_the_record,
                                       TabStorageType type,
                                       std::unique_ptr<StoragePackage> package)
    : StorageUpdateUnit(id),
      window_tag_(std::move(window_tag)),
      is_off_the_record_(is_off_the_record),
      type_(type),
      package_(std::move(package)) {}

SaveNodeUpdateUnit::~SaveNodeUpdateUnit() = default;

UnitType SaveNodeUpdateUnit::type() const {
  return UnitType::kSaveNode;
}

bool SaveNodeUpdateUnit::Execute(TabStateStorageDatabase* db,
                                 OpenTransaction* transaction) {
  std::vector<uint8_t> payload = package_->SerializePayload();
  std::vector<uint8_t> children = package_->SerializeChildren();
  bool success =
      db->SaveNode(transaction, id_, std::move(window_tag_), is_off_the_record_,
                   type_, std::move(payload), std::move(children));
  if (!success) {
    DLOG(ERROR) << "Could not perform save node operation.";
  }
  return success;
}

SavePayloadUpdateUnit::SavePayloadUpdateUnit(StorageId id,
                                             std::unique_ptr<Payload> payload)
    : StorageUpdateUnit(id), payload_(std::move(payload)) {}

SavePayloadUpdateUnit::~SavePayloadUpdateUnit() = default;

UnitType SavePayloadUpdateUnit::type() const {
  return UnitType::kSavePayload;
}

bool SavePayloadUpdateUnit::Execute(TabStateStorageDatabase* db,
                                    OpenTransaction* transaction) {
  std::vector<uint8_t> payload = payload_->SerializePayload();
  bool success = db->SaveNodePayload(transaction, id_, std::move(payload));
  if (!success) {
    DLOG(ERROR) << "Could not perform save node operation.";
  }
  return success;
}

SaveChildrenUpdateUnit::SaveChildrenUpdateUnit(
    StorageId id,
    std::unique_ptr<Payload> children)
    : StorageUpdateUnit(id), children_(std::move(children)) {}

SaveChildrenUpdateUnit::~SaveChildrenUpdateUnit() = default;

UnitType SaveChildrenUpdateUnit::type() const {
  return UnitType::kSaveChildren;
}

bool SaveChildrenUpdateUnit::Execute(TabStateStorageDatabase* db,
                                     OpenTransaction* transaction) {
  std::vector<uint8_t> serialized = children_->SerializePayload();
  bool success = db->SaveNodeChildren(transaction, id_, std::move(serialized));
  if (!success) {
    DLOG(ERROR) << "Could not perform save node children operation.";
  }
  return success;
}

RemoveNodeUpdateUnit::RemoveNodeUpdateUnit(StorageId id)
    : StorageUpdateUnit(id) {}

RemoveNodeUpdateUnit::~RemoveNodeUpdateUnit() = default;

UnitType RemoveNodeUpdateUnit::type() const {
  return UnitType::kRemoveNode;
}

bool RemoveNodeUpdateUnit::Execute(TabStateStorageDatabase* db,
                                   OpenTransaction* transaction) {
  bool success = db->RemoveNode(transaction, id_);
  if (!success) {
    DLOG(ERROR) << "Could not perform remove node operation.";
  }
  return success;
}

}  // namespace tabs
