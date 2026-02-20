// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_PENDING_UPDATES_H_
#define CHROME_BROWSER_TAB_STORAGE_PENDING_UPDATES_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/storage_update_unit.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "components/tabs/public/tab_collection.h"

namespace tabs {

enum class UnitType {
  kSaveNode = 0,
  kSavePayload = 1,
  kSaveChildren = 2,
  kRemoveNode = 3,
  kSaveDivergentChildren = 4,
};

class StorageUpdateUnit;

// An update request is used to separate the UI thread operations required to
// create a thread-agnostic StorageUpdateUnit.
class StoragePendingUpdate {
 public:
  explicit StoragePendingUpdate(StorageId id);
  virtual ~StoragePendingUpdate();

  // This must only be called on the UI Thread. Called only once per-instance.
  virtual std::unique_ptr<StorageUpdateUnit> CreateUnit() = 0;
  virtual UnitType type() const = 0;

 protected:
  const StorageId id_;
};

// StoragePendingUpdate to save a node.
class SaveNodePendingUpdate : public StoragePendingUpdate {
 public:
  SaveNodePendingUpdate(StorageId id,
                        std::string window_tag,
                        bool is_off_the_record,
                        TabStorageType type,
                        TabStoragePackager* packager,
                        StorageIdMapping& mapping,
                        TabCollectionNodeHandle handle);
  ~SaveNodePendingUpdate() override;

  std::unique_ptr<StorageUpdateUnit> CreateUnit() override;
  UnitType type() const override;

 private:
  std::string window_tag_;
  const bool is_off_the_record_;
  const TabStorageType type_;
  raw_ptr<TabStoragePackager> packager_;
  raw_ref<StorageIdMapping> mapping_;
  const TabCollectionNodeHandle handle_;
};

// StoragePendingUpdate to save a payload.
class SavePayloadPendingUpdate : public StoragePendingUpdate {
 public:
  SavePayloadPendingUpdate(StorageId id,
                           std::string window_tag,
                           bool is_off_the_record,
                           TabStoragePackager* packager,
                           StorageIdMapping& mapping,
                           TabCollectionNodeHandle handle);
  ~SavePayloadPendingUpdate() override;

  std::unique_ptr<StorageUpdateUnit> CreateUnit() override;
  UnitType type() const override;

 private:
  std::string window_tag_;
  const bool is_off_the_record_;
  raw_ref<StorageIdMapping> mapping_;
  raw_ptr<TabStoragePackager> packager_;
  const TabCollectionNodeHandle handle_;
};

// StoragePendingUpdate to save children.
class SaveChildrenPendingUpdate : public StoragePendingUpdate {
 public:
  SaveChildrenPendingUpdate(StorageId id,
                            TabStoragePackager* packager,
                            StorageIdMapping& mapping,
                            TabCollectionHandle handle);
  ~SaveChildrenPendingUpdate() override;

  std::unique_ptr<StorageUpdateUnit> CreateUnit() override;
  UnitType type() const override;

 private:
  raw_ptr<TabStoragePackager> packager_;
  raw_ref<StorageIdMapping> mapping_;
  const TabCollectionHandle handle_;
};

// StoragePendingUpdate to save divergent children.
class SaveDivergentChildrenPendingUpdate : public StoragePendingUpdate {
 public:
  SaveDivergentChildrenPendingUpdate(StorageId id,
                                     std::string window_tag,
                                     bool is_off_the_record,
                                     TabStoragePackager* packager,
                                     StorageIdMapping& mapping,
                                     TabCollectionHandle handle);
  ~SaveDivergentChildrenPendingUpdate() override;

  std::unique_ptr<StorageUpdateUnit> CreateUnit() override;
  UnitType type() const override;

 private:
  std::string window_tag_;
  const bool is_off_the_record_;
  raw_ptr<TabStoragePackager> packager_;
  raw_ref<StorageIdMapping> mapping_;
  const TabCollectionHandle handle_;
};

// StoragePendingUpdate to remove a node.
class RemoveNodePendingUpdate : public StoragePendingUpdate {
 public:
  explicit RemoveNodePendingUpdate(StorageId id);
  ~RemoveNodePendingUpdate() override;

  std::unique_ptr<StorageUpdateUnit> CreateUnit() override;
  UnitType type() const override;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_PENDING_UPDATES_H_
