// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_updater_builder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/tab/storage_update_unit.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_state_storage_updater.h"
#include "chrome/browser/tab/tab_storage_package.h"

namespace tabs {

namespace {

class SaveNodeUpdateUnit : public tabs::StorageUpdateUnit {
 public:
  SaveNodeUpdateUnit(int id,
                     tabs::TabStorageType type,
                     std::unique_ptr<tabs::StoragePackage> package)
      : id_(id), type_(type), package_(std::move(package)) {}

  bool Execute(
      tabs::TabStateStorageDatabase* db,
      tabs::TabStateStorageDatabase::Transaction* transaction) override {
    std::string payload = package_->SerializePayload();
    std::string children = package_->SerializeChildren();
    bool success = db->SaveNode(transaction, id_, type_, std::move(payload),
                                std::move(children));
    if (!success) {
      DLOG(ERROR) << "Could not perform save node operation.";
    }
    return success;
  }

 private:
  int id_;
  tabs::TabStorageType type_;
  std::unique_ptr<tabs::StoragePackage> package_;
};

class SaveChildrenUpdateUnit : public tabs::StorageUpdateUnit {
 public:
  SaveChildrenUpdateUnit(int id, std::unique_ptr<tabs::Payload> children)
      : id_(id), children_(std::move(children)) {}

  bool Execute(
      tabs::TabStateStorageDatabase* db,
      tabs::TabStateStorageDatabase::Transaction* transaction) override {
    std::string serialized = children_->SerializePayload();
    bool success =
        db->SaveNodeChildren(transaction, id_, std::move(serialized));
    if (!success) {
      DLOG(ERROR) << "Could not perform save node children operation.";
    }
    return success;
  }

 private:
  int id_;
  std::unique_ptr<tabs::Payload> children_;
};

class RemoveNodeUpdateUnit : public tabs::StorageUpdateUnit {
 public:
  explicit RemoveNodeUpdateUnit(int id) : id_(id) {}

  bool Execute(
      tabs::TabStateStorageDatabase* db,
      tabs::TabStateStorageDatabase::Transaction* transaction) override {
    bool success = db->RemoveNode(transaction, id_);
    if (!success) {
      DLOG(ERROR) << "Could not perform remove node operation.";
    }
    return success;
  }

 private:
  int id_;
};

}  // namespace

TabStateStorageUpdaterBuilder::TabStateStorageUpdaterBuilder()
    : updater_(std::make_unique<TabStateStorageUpdater>()) {}

TabStateStorageUpdaterBuilder::~TabStateStorageUpdaterBuilder() = default;

void TabStateStorageUpdaterBuilder::SaveNode(
    int id,
    TabStorageType type,
    std::unique_ptr<StoragePackage> package) {
  updater_->Add(
      std::make_unique<SaveNodeUpdateUnit>(id, type, std::move(package)));
}

void TabStateStorageUpdaterBuilder::SaveChildren(
    int id,
    std::unique_ptr<Payload> children) {
  updater_->Add(
      std::make_unique<SaveChildrenUpdateUnit>(id, std::move(children)));
}

void TabStateStorageUpdaterBuilder::RemoveNode(int id) {
  updater_->Add(std::make_unique<RemoveNodeUpdateUnit>(id));
}

std::unique_ptr<TabStateStorageUpdater> TabStateStorageUpdaterBuilder::Build() {
  return std::move(updater_);
}

}  // namespace tabs
