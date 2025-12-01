// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_UPDATE_UNITS_H_
#define CHROME_BROWSER_TAB_STORAGE_UPDATE_UNITS_H_

#include <memory>
#include <string>

#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_update_unit.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "chrome/browser/tab/tab_storage_type.h"

namespace tabs {

// StorageUpdateUnit to save a node.
class SaveNodeUpdateUnit : public StorageUpdateUnit {
 public:
  SaveNodeUpdateUnit(StorageId id,
                     std::string window_tag,
                     bool is_off_the_record,
                     TabStorageType type,
                     std::unique_ptr<StoragePackage> package);
  ~SaveNodeUpdateUnit() override;

  // StorageUpdateUnit
  bool Execute(TabStateStorageDatabase* db,
               TabStateStorageDatabase::OpenTransaction* transaction) override;

 private:
  const StorageId id_;
  std::string window_tag_;
  const bool is_off_the_record_;
  const TabStorageType type_;
  std::unique_ptr<StoragePackage> package_;
};

// StorageUpdateUnit to save a payload.
class SavePayloadUpdateUnit : public StorageUpdateUnit {
 public:
  SavePayloadUpdateUnit(StorageId id, std::unique_ptr<Payload> payload);
  ~SavePayloadUpdateUnit() override;

  // StorageUpdateUnit
  bool Execute(TabStateStorageDatabase* db,
               TabStateStorageDatabase::OpenTransaction* transaction) override;

 private:
  const StorageId id_;
  std::unique_ptr<Payload> payload_;
};

// StorageUpdateUnit to save children.
class SaveChildrenUpdateUnit : public StorageUpdateUnit {
 public:
  SaveChildrenUpdateUnit(StorageId id, std::unique_ptr<Payload> children);
  ~SaveChildrenUpdateUnit() override;

  // StorageUpdateUnit
  bool Execute(TabStateStorageDatabase* db,
               TabStateStorageDatabase::OpenTransaction* transaction) override;

 private:
  const StorageId id_;
  std::unique_ptr<Payload> children_;
};

// StorageUpdateUnit to remove a node.
class RemoveNodeUpdateUnit : public StorageUpdateUnit {
 public:
  explicit RemoveNodeUpdateUnit(StorageId id);
  ~RemoveNodeUpdateUnit() override;

  // StorageUpdateUnit
  bool Execute(TabStateStorageDatabase* db,
               TabStateStorageDatabase::OpenTransaction* transaction) override;

 private:
  const StorageId id_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_UPDATE_UNITS_H_
