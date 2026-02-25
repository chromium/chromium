// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_RESTORE_ORCHESTRATOR_H_
#define CHROME_BROWSER_TAB_STORAGE_RESTORE_ORCHESTRATOR_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/tab/collection_storage_observer.h"
#include "chrome/browser/tab/storage_collection_synchronizer.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_collection_observer.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {

// Orchestrates the restoration of a TabStripCollection from storage.
// Differentiates between model changes resulting from the restoration process
// and changes as a result of user actions during the restoration
// process.
// Without the use of batching, this is inefficient. See
// TabStateStorageService#CreateScopedBatch.
class StorageRestoreOrchestrator
    : public StorageCollectionSynchronizer::CollectionSynchronizerObserver {
 public:
  StorageRestoreOrchestrator(TabStripCollection* collection,
                             TabStateStorageService* service,
                             StorageLoadedData* loaded_data);
  ~StorageRestoreOrchestrator() override;

  StorageRestoreOrchestrator(const StorageRestoreOrchestrator&) = delete;
  StorageRestoreOrchestrator& operator=(const StorageRestoreOrchestrator&) =
      delete;

  // CollectionSynchronizerObserver:
  void OnChildrenAdded(const TabCollection::Position& position,
                       const TabCollectionNodes& handles,
                       bool insert_from_detached) override;
  void OnChildrenRemoved(const TabCollection::Position& position,
                         const TabCollectionNodes& handles) override;
  void OnChildMoved(const TabCollection::Position& to_position,
                    const NodeData& node_data) override;
  void SaveChildNodeOnly(TabCollectionNodeHandle handle) override;

  void OnNodeRejected(StorageId node);
  void OnRestoreCancelled() override;

 private:
  class ObserverImpl : public StorageLoadedData::Observer {
   public:
    explicit ObserverImpl(StorageRestoreOrchestrator* orchestrator);
    ~ObserverImpl() override;
    void OnNodeRejected(StorageId node) override;

   private:
    raw_ptr<StorageRestoreOrchestrator> orchestrator_;
  };

  void OnSaveChildTab(const TabCollection::NodeHandle& handle,
                      bool was_inserted);
  void OnSaveChildCollection(const TabCollection::NodeHandle& handle,
                             bool was_inserted);

  // Tracks events performed on StorageLoadedData.
  ObserverImpl data_observer_;

  raw_ptr<TabStripCollection> collection_;
  raw_ptr<TabStateStorageService> service_;
  raw_ptr<StorageLoadedData> loaded_data_;

  bool is_restore_cancelled_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_RESTORE_ORCHESTRATOR_H_
