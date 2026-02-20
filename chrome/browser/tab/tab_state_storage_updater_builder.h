// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_

#include <initializer_list>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/storage_pending_updates.h"
#include "chrome/browser/tab/tab_state_storage_updater.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "components/tabs/public/tab_collection.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace tabs {

// Builder for TabStateStorageUpdater.
class TabStateStorageUpdaterBuilder {
 public:
  TabStateStorageUpdaterBuilder(StorageIdMapping& mapping,
                                TabStoragePackager* packager);
  ~TabStateStorageUpdaterBuilder();

  TabStateStorageUpdaterBuilder(const TabStateStorageUpdaterBuilder&) = delete;
  TabStateStorageUpdaterBuilder& operator=(
      const TabStateStorageUpdaterBuilder&) = delete;

  TabStateStorageUpdaterBuilder(TabStateStorageUpdaterBuilder&&);
  TabStateStorageUpdaterBuilder& operator=(TabStateStorageUpdaterBuilder&&);

  void SaveNode(StorageId id,
                std::string window_tag,
                bool is_off_the_record,
                TabStorageType type,
                TabCollectionNodeHandle handle);
  void SaveNodePayload(StorageId id, TabCollectionNodeHandle handle);
  // Use a pointer instead of a handle, since converting back to a pointer can
  // be slow.
  void SaveChildren(StorageId id, const TabCollection* collection);
  void SaveDivergentChildren(StorageId id, const TabCollection* collection);
  void RemoveNode(StorageId id);

  std::unique_ptr<TabStateStorageUpdater> Build();

 private:
  // Returns true if an update for `id` exists and its type is one of `types`.
  bool ContainsUpdateWithAnyType(StorageId id,
                                 std::initializer_list<UnitType> types);
  // Helper to squash save payload and save children updates into a single save
  // node update.
  void SquashIntoSaveNode(StorageId id, const TabCollection* collection);

  raw_ref<StorageIdMapping> mapping_;
  raw_ptr<TabStoragePackager> packager_;
  absl::flat_hash_map<StorageId, std::unique_ptr<StoragePendingUpdate>>
      update_for_id_;
  absl::flat_hash_map<StorageId, std::unique_ptr<StoragePendingUpdate>>
      divergence_update_for_id_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_UPDATER_BUILDER_H_
