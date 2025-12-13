// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_updater.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/tab/storage_update_unit.h"
#include "chrome/browser/tab/tab_state_storage_database.h"

namespace tabs {

using OpenTransaction = TabStateStorageDatabase::OpenTransaction;

TabStateStorageUpdater::TabStateStorageUpdater() = default;
TabStateStorageUpdater::~TabStateStorageUpdater() = default;

void TabStateStorageUpdater::Add(std::unique_ptr<StorageUpdateUnit> unit) {
  updates_.push_back(std::move(unit));
}

bool TabStateStorageUpdater::Execute(TabStateStorageDatabase* db) {
  OpenTransaction* transaction = db->CreateTransaction();

  if (!transaction->HasFailed()) {
    for (auto& op : updates_) {
      if (!op->Execute(db, transaction)) {
        transaction->MarkFailed();
        break;
      }
    }
  }

  return db->CloseTransaction(transaction);
}

}  // namespace tabs
