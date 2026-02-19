// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_LOADED_DATA_H_
#define CHROME_BROWSER_TAB_STORAGE_LOADED_DATA_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace tabs {

class RestoreEntityTracker;
class TabStateStorageDatabase;

// Represents data loaded from the database.
class StorageLoadedData {
 public:
  // Holds the error state for storage loading. Will be successful, unless
  // marked otherwise.
  class StorageLoadingContext {
   public:
    StorageLoadingContext();
    ~StorageLoadingContext();

    StorageLoadingContext(const StorageLoadingContext&) = delete;
    StorageLoadingContext& operator=(const StorageLoadingContext&) = delete;

    StorageLoadingContext(StorageLoadingContext&&);
    StorageLoadingContext& operator=(StorageLoadingContext&&);

    void SetStatus(StorageLoadingStatus status, std::string message);
    bool HasError() const;

    StorageLoadingStatus status() const;
    const std::optional<std::string>& error_message() const;

   private:
    StorageLoadingStatus status_ = StorageLoadingStatus::kSuccess;
    std::optional<std::string> error_message_;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when the child of a node is rejected. This occurs when the child
    // is declared invalid or unneeded after loading it.
    virtual void OnChildRejected(StorageId parent) = 0;

    // Called when the StorageLoadedData is destroyed.
    virtual void OnDestroyed() = 0;
  };

  ~StorageLoadedData();

  StorageLoadedData(const StorageLoadedData&) = delete;
  StorageLoadedData& operator=(const StorageLoadedData&) = delete;

  StorageLoadedData(StorageLoadedData&&) = delete;
  StorageLoadedData& operator=(StorageLoadedData&&) = delete;

  class Builder {
   public:
    explicit Builder(std::unique_ptr<RestoreEntityTracker> tracker);
    ~Builder();

    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    Builder(Builder&&);
    Builder& operator=(Builder&&);

    // Called on the DB task runner to process a node.
    void AddNode(StorageId id,
                 TabStorageType type,
                 base::span<const uint8_t> payload,
                 std::optional<base::span<const uint8_t>> children,
                 base::PassKey<TabStateStorageDatabase>);
    std::unique_ptr<StorageLoadedData> Build();

   private:
    std::unique_ptr<RestoreEntityTracker> tracker_;
    absl::flat_hash_map<StorageId, tabs_pb::TabState> loaded_tabs_map_;
    absl::flat_hash_map<StorageId, std::vector<StorageId>> children_map_;
    std::vector<std::unique_ptr<TabGroupCollectionData>> loaded_groups_;
    std::optional<StorageId> root_storage_id_;
    std::optional<StorageId> active_tab_storage_id_;
    StorageLoadingContext context_;
  };

  RestoreEntityTracker* GetTracker() const;
  std::vector<tabs_pb::TabState>& GetLoadedTabs();
  std::vector<std::unique_ptr<TabGroupCollectionData>>& GetLoadedGroups();
  std::optional<int> GetActiveTabIndex() const;

  const StorageLoadingContext& GetLoadingContext() const;

  // Alerts observers that a child node has been rejected during the restoration
  // process.
  void NotifyChildRejected(StorageId parent);

  void RegisterObserver(Observer* observer);
  void UnregisterObserver(Observer* observer);

 private:
  friend class Builder;

  StorageLoadedData(
      std::vector<tabs_pb::TabState> loaded_tabs,
      std::vector<std::unique_ptr<TabGroupCollectionData>> loaded_groups,
      std::unique_ptr<RestoreEntityTracker> associator,
      std::optional<int> active_tab_index,
      StorageLoadingContext context);

  base::ObserverList<Observer> observers_;
  std::vector<tabs_pb::TabState> loaded_tabs_;
  std::vector<std::unique_ptr<TabGroupCollectionData>> loaded_groups_;
  std::unique_ptr<RestoreEntityTracker> tracker_;
  std::optional<int> active_tab_index_;
  StorageLoadingContext context_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_LOADED_DATA_H_
