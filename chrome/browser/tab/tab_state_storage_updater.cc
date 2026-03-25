// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_updater.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/tab/storage_update_unit.h"
#include "chrome/browser/tab/tab_state_storage_database.h"

namespace tabs {

using OpenTransaction = TabStateStorageDatabase::OpenTransaction;

TabStateStorageUpdater::TabStateStorageUpdater(
    std::vector<std::unique_ptr<StorageUpdateUnit>> updates,
    std::vector<base::OnceClosure> callbacks)
    : updates_(std::move(updates)), callbacks_(std::move(callbacks)) {}
TabStateStorageUpdater::~TabStateStorageUpdater() = default;

bool TabStateStorageUpdater::Execute(TabStateStorageDatabase* db) {
  OpenTransaction* transaction = db->CreateTransaction();

  for (auto& callback : callbacks_) {
    transaction->AddCallback(std::move(callback));
  }

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
