// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/tab/storage_update_unit.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "chrome/browser/tab/tab_storage_type.h"

namespace tabs {

using Transaction = TabStateStorageDatabase::Transaction;

// Atomically performs a batch of updates in storage.
class TabStateStorageUpdater {
 public:
  explicit TabStateStorageUpdater(TabStateStorageDatabase* db);
  TabStateStorageUpdater(const TabStateStorageUpdater&) = delete;
  TabStateStorageUpdater& operator=(const TabStateStorageUpdater&) = delete;
  ~TabStateStorageUpdater();

  void Add(std::unique_ptr<StorageUpdateUnit> unit);
  bool PerformUpdate();

 private:
  raw_ptr<TabStateStorageDatabase> db_;

  std::vector<std::unique_ptr<StorageUpdateUnit>> updates_;
  std::unique_ptr<Transaction> transaction_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_H_
