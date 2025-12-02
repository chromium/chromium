// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_UPDATE_UNIT_H_
#define CHROME_BROWSER_TAB_STORAGE_UPDATE_UNIT_H_

#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/tab_state_storage_database.h"

namespace tabs {

enum class UnitType;

// Represents a single update operation to the storage layer.
class StorageUpdateUnit {
 public:
  explicit StorageUpdateUnit(StorageId id);
  virtual ~StorageUpdateUnit() = default;

  StorageId id() const { return id_; }
  virtual UnitType type() const = 0;

  // Returns false if the update operation failed.
  virtual bool Execute(
      TabStateStorageDatabase* db,
      TabStateStorageDatabase::OpenTransaction* transaction) = 0;

 protected:
  const StorageId id_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_UPDATE_UNIT_H_
