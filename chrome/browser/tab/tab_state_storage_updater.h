// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"

namespace tabs {

class StorageUpdateUnit;
class TabStateStorageDatabase;

// Atomically performs a batch of updates in storage.
class TabStateStorageUpdater {
 public:
  TabStateStorageUpdater();
  TabStateStorageUpdater(const TabStateStorageUpdater&) = delete;
  TabStateStorageUpdater& operator=(const TabStateStorageUpdater&) = delete;
  ~TabStateStorageUpdater();

  void Add(std::unique_ptr<StorageUpdateUnit> unit);
  bool Execute(TabStateStorageDatabase* db);

 private:
  std::vector<std::unique_ptr<StorageUpdateUnit>> updates_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_H_
