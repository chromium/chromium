// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_pending_updates.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/storage_package.h"
#include "chrome/browser/tab/storage_update_unit.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

namespace {

using OpenTransaction = TabStateStorageDatabase::OpenTransaction;

// StorageUpdateUnit to save a node.
class SaveNodeUpdateUnit : public StorageUpdateUnit {
 public:
  SaveNodeUpdateUnit(StorageId id,
                     std::string window_tag,
                     bool is_off_the_record,
                     TabStorageType type,
                     std::unique_ptr<StoragePackage> package)
      : id_(id),
        window_tag_(std::move(window_tag)),
        is_off_the_record_(is_off_the_record),
        type_(type),
        package_(std::move(package)) {}

  ~SaveNodeUpdateUnit() override = default;

  bool Execute(TabStateStorageDatabase* db,
               OpenTransaction* transaction) override {
    std::vector<uint8_t> payload = package_->SerializePayload();
    std::vector<uint8_t> children = package_->SerializeChildren();
    bool success = db->SaveNode(transaction, id_, std::move(window_tag_),
                                is_off_the_record_, type_, std::move(payload),
                                std::move(children));
    if (!success) {
      DLOG(ERROR) << "Could not perform node update operation.";
    }
    return success;
  }

 private:
  StorageId id_;
  std::string window_tag_;
  const bool is_off_the_record_;
  const TabStorageType type_;
  std::unique_ptr<StoragePackage> package_;
};

// StorageUpdateUnit to save a payload.
class SavePayloadUpdateUnit : public StorageUpdateUnit {
 public:
  SavePayloadUpdateUnit(StorageId id,
                        std::string window_tag,
                        bool is_off_the_record,
                        std::unique_ptr<Payload> payload)
      : id_(id),
        window_tag_(std::move(window_tag)),
        is_off_the_record_(is_off_the_record),
        payload_(std::move(payload)) {}

  ~SavePayloadUpdateUnit() override = default;

  bool Execute(TabStateStorageDatabase* db,
               OpenTransaction* transaction) override {
    bool success =
        db->SaveNodePayload(transaction, id_, window_tag_, is_off_the_record_,
                            payload_->SerializePayload());
    if (!success) {
      DLOG(ERROR) << "Could not perform node payload update operation.";
    }
    return success;
  }

 private:
  StorageId id_;
  std::string window_tag_;
  const bool is_off_the_record_;
  std::unique_ptr<Payload> payload_;
};

// StorageUpdateUnit to save children.
class SaveChildrenUpdateUnit : public StorageUpdateUnit {
 public:
  SaveChildrenUpdateUnit(StorageId id, std::unique_ptr<Payload> children)
      : id_(id), children_(std::move(children)) {}

  ~SaveChildrenUpdateUnit() override = default;

  bool Execute(TabStateStorageDatabase* db,
               OpenTransaction* transaction) override {
    bool success =
        db->SaveNodeChildren(transaction, id_, children_->SerializePayload());
    if (!success) {
      DLOG(ERROR) << "Could not perform save node children operation.";
    }
    return success;
  }

 private:
  StorageId id_;
  std::unique_ptr<Payload> children_;
};

// StorageUpdateUnit to save divergent children.
class SaveDivergentChildrenUpdateUnit : public StorageUpdateUnit {
 public:
  SaveDivergentChildrenUpdateUnit(StorageId id,
                                  std::string window_tag,
                                  bool is_off_the_record,
                                  std::unique_ptr<Payload> children)
      : id_(id),
        window_tag_(std::move(window_tag)),
        is_off_the_record_(is_off_the_record),
        children_(std::move(children)) {}

  ~SaveDivergentChildrenUpdateUnit() override = default;

  bool Execute(TabStateStorageDatabase* db,
               OpenTransaction* transaction) override {
    bool success =
        db->SaveDivergentNode(transaction, id_, window_tag_, is_off_the_record_,
                              children_->SerializePayload());
    if (!success) {
      DLOG(ERROR) << "Could not perform save divergent children operation.";
    }
    return success;
  }

 private:
  StorageId id_;
  std::string window_tag_;
  const bool is_off_the_record_;
  std::unique_ptr<Payload> children_;
};

// StorageUpdateUnit to remove a node.
class RemoveNodeUpdateUnit : public StorageUpdateUnit {
 public:
  explicit RemoveNodeUpdateUnit(StorageId id) : id_(id) {}
  ~RemoveNodeUpdateUnit() override = default;

  bool Execute(TabStateStorageDatabase* db,
               OpenTransaction* transaction) override {
    bool success = db->RemoveNode(transaction, id_);
    if (!success) {
      DLOG(ERROR) << "Could not perform remove node operation.";
    }
    return success;
  }

 private:
  StorageId id_;
};

}  // namespace

StoragePendingUpdate::StoragePendingUpdate(StorageId id) : id_(id) {}

StoragePendingUpdate::~StoragePendingUpdate() = default;

SaveNodePendingUpdate::SaveNodePendingUpdate(StorageId id,
                                             std::string window_tag,
                                             bool is_off_the_record,
                                             TabStorageType type,
                                             TabStoragePackager* packager,
                                             StorageIdMapping& mapping,
                                             TabCollectionNodeHandle handle)
    : StoragePendingUpdate(id),
      window_tag_(std::move(window_tag)),
      is_off_the_record_(is_off_the_record),
      type_(type),
      packager_(packager),
      mapping_(mapping),
      handle_(std::move(handle)) {}

SaveNodePendingUpdate::~SaveNodePendingUpdate() = default;

UnitType SaveNodePendingUpdate::type() const {
  return UnitType::kSaveNode;
}

std::unique_ptr<StorageUpdateUnit> SaveNodePendingUpdate::CreateUnit() {
  std::unique_ptr<StoragePackage> package;
  if (std::holds_alternative<TabCollectionHandle>(handle_)) {
    TabCollectionHandle collection_handle =
        std::get<TabCollectionHandle>(handle_);
    package = packager_->Package(collection_handle.Get(), mapping_.get());
  } else {
    TabHandle tab_handle = std::get<TabHandle>(handle_);
    package = packager_->Package(tab_handle.Get());
  }

  return std::make_unique<SaveNodeUpdateUnit>(id_, std::move(window_tag_),
                                              is_off_the_record_, type_,
                                              std::move(package));
}

SavePayloadPendingUpdate::SavePayloadPendingUpdate(
    StorageId id,
    std::string window_tag,
    bool is_off_the_record,
    TabStoragePackager* packager,
    StorageIdMapping& mapping,
    TabCollectionNodeHandle handle)
    : StoragePendingUpdate(id),
      window_tag_(std::move(window_tag)),
      is_off_the_record_(is_off_the_record),
      mapping_(mapping),
      packager_(packager),
      handle_(std::move(handle)) {}

SavePayloadPendingUpdate::~SavePayloadPendingUpdate() = default;

UnitType SavePayloadPendingUpdate::type() const {
  return UnitType::kSavePayload;
}

std::unique_ptr<StorageUpdateUnit> SavePayloadPendingUpdate::CreateUnit() {
  std::unique_ptr<Payload> payload;
  if (std::holds_alternative<TabCollectionHandle>(handle_)) {
    TabCollectionHandle collection_handle =
        std::get<TabCollectionHandle>(handle_);
    payload =
        packager_->PackagePayload(collection_handle.Get(), mapping_.get());
  } else {
    TabHandle tab_handle = std::get<TabHandle>(handle_);
    payload = packager_->Package(tab_handle.Get());
  }
  return std::make_unique<SavePayloadUpdateUnit>(
      id_, std::move(window_tag_), is_off_the_record_, std::move(payload));
}

SaveChildrenPendingUpdate::SaveChildrenPendingUpdate(
    StorageId id,
    TabStoragePackager* packager,
    StorageIdMapping& mapping,
    TabCollectionHandle handle)
    : StoragePendingUpdate(id),
      packager_(packager),
      mapping_(mapping),
      handle_(std::move(handle)) {}

SaveChildrenPendingUpdate::~SaveChildrenPendingUpdate() = default;

UnitType SaveChildrenPendingUpdate::type() const {
  return UnitType::kSaveChildren;
}

std::unique_ptr<StorageUpdateUnit> SaveChildrenPendingUpdate::CreateUnit() {
  return std::make_unique<SaveChildrenUpdateUnit>(
      id_, packager_->PackageChildren(handle_.Get(), mapping_.get()));
}

SaveDivergentChildrenPendingUpdate::SaveDivergentChildrenPendingUpdate(
    StorageId id,
    std::string window_tag,
    bool is_off_the_record,
    TabStoragePackager* packager,
    StorageIdMapping& mapping,
    TabCollectionHandle handle)
    : StoragePendingUpdate(id),
      window_tag_(std::move(window_tag)),
      is_off_the_record_(is_off_the_record),
      packager_(packager),
      mapping_(mapping),
      handle_(std::move(handle)) {}

SaveDivergentChildrenPendingUpdate::~SaveDivergentChildrenPendingUpdate() =
    default;

UnitType SaveDivergentChildrenPendingUpdate::type() const {
  return UnitType::kSaveDivergentChildren;
}

std::unique_ptr<StorageUpdateUnit>
SaveDivergentChildrenPendingUpdate::CreateUnit() {
  return std::make_unique<SaveDivergentChildrenUpdateUnit>(
      id_, std::move(window_tag_), is_off_the_record_,
      packager_->PackageChildren(handle_.Get(), mapping_.get()));
}

RemoveNodePendingUpdate::RemoveNodePendingUpdate(StorageId id)
    : StoragePendingUpdate(id) {}

RemoveNodePendingUpdate::~RemoveNodePendingUpdate() = default;

UnitType RemoveNodePendingUpdate::type() const {
  return UnitType::kRemoveNode;
}

std::unique_ptr<StorageUpdateUnit> RemoveNodePendingUpdate::CreateUnit() {
  return std::make_unique<RemoveNodeUpdateUnit>(id_);
}

}  // namespace tabs
