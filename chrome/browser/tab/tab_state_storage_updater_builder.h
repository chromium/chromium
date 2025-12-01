// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_

#include <memory>
#include <string>

#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_update_units.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "chrome/browser/tab/tab_storage_type.h"

namespace tabs {

class TabStateStorageUpdater;

// Builder for TabStateStorageUpdater.
class TabStateStorageUpdaterBuilder {
 public:
  TabStateStorageUpdaterBuilder();
  TabStateStorageUpdaterBuilder(const TabStateStorageUpdaterBuilder&) = delete;
  TabStateStorageUpdaterBuilder& operator=(
      const TabStateStorageUpdaterBuilder&) = delete;
  ~TabStateStorageUpdaterBuilder();

  void SaveNode(StorageId id,
                std::string window_tag,
                bool is_off_the_record,
                TabStorageType type,
                std::unique_ptr<StoragePackage> package);
  void SaveNodePayload(StorageId id, std::unique_ptr<Payload> payload);
  void SaveChildren(StorageId id, std::unique_ptr<Payload> children);
  void RemoveNode(StorageId id);

  std::unique_ptr<TabStateStorageUpdater> Build();

 private:
  std::unique_ptr<TabStateStorageUpdater> updater_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_
