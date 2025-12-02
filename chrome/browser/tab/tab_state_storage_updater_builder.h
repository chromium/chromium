// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "components/tabs/public/tab_collection.h"

namespace tabs {

class TabStateStorageUpdater;

// Builder for TabStateStorageUpdater.
class TabStateStorageUpdaterBuilder {
 public:
  TabStateStorageUpdaterBuilder(StorageIdMapping& mapping,
                                TabStoragePackager* packager);
  TabStateStorageUpdaterBuilder(const TabStateStorageUpdaterBuilder&) = delete;
  TabStateStorageUpdaterBuilder& operator=(
      const TabStateStorageUpdaterBuilder&) = delete;
  ~TabStateStorageUpdaterBuilder();

  void SaveNode(StorageId id,
                std::string window_tag,
                bool is_off_the_record,
                TabStorageType type,
                TabCollectionNodeHandle handle);
  void SaveNodePayload(StorageId id, TabCollectionNodeHandle handle);
  void SaveChildren(StorageId id, TabCollectionHandle handle);
  void RemoveNode(StorageId id);

  std::unique_ptr<TabStateStorageUpdater> Build();

 private:
  raw_ref<StorageIdMapping> mapping_;
  raw_ptr<TabStoragePackager> packager_;
  std::unique_ptr<TabStateStorageUpdater> updater_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_
