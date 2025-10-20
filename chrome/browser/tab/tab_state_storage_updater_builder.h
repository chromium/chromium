// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_

#include <memory>

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

  void SaveNode(int id,
                TabStorageType type,
                std::unique_ptr<StoragePackage> package);
  void SaveChildren(int id, std::unique_ptr<Payload> children);
  void RemoveNode(int id);

  std::unique_ptr<TabStateStorageUpdater> Build();

 private:
  std::unique_ptr<TabStateStorageUpdater> updater_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_
