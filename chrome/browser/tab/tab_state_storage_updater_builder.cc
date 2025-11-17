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
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_state_storage_updater.h"
#include "chrome/browser/tab/tab_storage_package.h"

namespace tabs {

using OpenTransaction = TabStateStorageDatabase::OpenTransaction;

namespace {

class SaveNodeUpdateUnit : public StorageUpdateUnit {
 public:
  SaveNodeUpdateUnit(int id,
                     std::string window_tag,
                     bool is_off_the_record,
                     TabStorageType type,
                     std::unique_ptr<StoragePackage> package)
      : id_(id),
        window_tag_(std::move(window_tag)),
        is_off_the_record_(is_off_the_record),
        type_(type),
        package_(std::move(package)) {}

  bool Execute(TabStateStorageDatabase* db,
               OpenTransaction* transaction) override {
    std::vector<uint8_t> payload = package_->SerializePayload();
    std::vector<uint8_t> children = package_->SerializeChildren();
    bool success = db->SaveNode(transaction, id_, std::move(window_tag_),
                                is_off_the_record_, type_, std::move(payload),
                                std::move(children));
    if (!success) {
      DLOG(ERROR) << "Could not perform save node operation.";
    }
    return success;
  }

 private:
  int id_;
  std::string window_tag_;
  bool is_off_the_record_;
  TabStorageType type_;
  std::unique_ptr<StoragePackage> package_;
};

class SavePayloadUpdateUnit : public StorageUpdateUnit {
 public:
  SavePayloadUpdateUnit(int id, std::unique_ptr<Payload> payload)
      : id_(id), payload_(std::move(payload)) {}

  bool Execute(TabStateStorageDatabase* db,
               OpenTransaction* transaction) override {
    std::vector<uint8_t> payload = payload_->SerializePayload();
    bool success = db->SaveNodePayload(transaction, id_, std::move(payload));
    if (!success) {
      DLOG(ERROR) << "Could not perform save node operation.";
    }
    return success;
  }

 private:
  int id_;
  std::unique_ptr<Payload> payload_;
};

class SaveChildrenUpdateUnit : public StorageUpdateUnit {
 public:
  SaveChildrenUpdateUnit(int id, std::unique_ptr<Payload> children)
      : id_(id), children_(std::move(children)) {}

  bool Execute(TabStateStorageDatabase* db,
               OpenTransaction* transaction) override {
    std::vector<uint8_t> serialized = children_->SerializePayload();
    bool success =
        db->SaveNodeChildren(transaction, id_, std::move(serialized));
    if (!success) {
      DLOG(ERROR) << "Could not perform save node children operation.";
    }
    return success;
  }

 private:
  int id_;
  std::unique_ptr<Payload> children_;
};

class RemoveNodeUpdateUnit : public StorageUpdateUnit {
 public:
  explicit RemoveNodeUpdateUnit(int id) : id_(id) {}

  bool Execute(TabStateStorageDatabase* db,
               OpenTransaction* transaction) override {
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
    std::string window_tag,
    bool is_off_the_record,
    TabStorageType type,
    std::unique_ptr<StoragePackage> package) {
  updater_->Add(std::make_unique<SaveNodeUpdateUnit>(
      id, std::move(window_tag), is_off_the_record, type, std::move(package)));
}

void TabStateStorageUpdaterBuilder::SaveNodePayload(
    int id,
    std::unique_ptr<Payload> payload) {
  updater_->Add(
      std::make_unique<SavePayloadUpdateUnit>(id, std::move(payload)));
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
