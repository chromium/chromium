// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_BACKEND_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_BACKEND_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_state_storage_updater.h"

namespace tabs {

// Backend for TabStateStorage, responsible for coordinating with the storage
// layer.
class TabStateStorageBackend {
 public:
  TabStateStorageBackend(const base::FilePath& profile_path,
                         bool support_off_the_record_data);
  TabStateStorageBackend(const TabStateStorageBackend&) = delete;
  TabStateStorageBackend& operator=(const TabStateStorageBackend&) = delete;
  ~TabStateStorageBackend();

  void Initialize();

  // Boosts the priority of the database operations to USER_BLOCKING until all
  // current pending operations are complete. This should be used when it is
  // critical to save user data.
  void BoostPriority();

  void WaitForAllPendingOperations(base::OnceClosure on_idle);

  // Performs an atomic database update.
  void Update(std::unique_ptr<TabStateStorageUpdater> updater);

  using OnStorageLoadedData =
      base::OnceCallback<void(std::unique_ptr<StorageLoadedData>)>;
  void LoadAllNodes(std::string_view window_tag,
                    bool is_off_the_record,
                    std::unique_ptr<StorageLoadedData::Builder> builder,
                    OnStorageLoadedData on_storage_loaded_data);

  using OnCountTabsForWindow = base::OnceCallback<void(int)>;
  void CountTabsForWindow(std::string_view window_tag,
                          bool is_off_the_record,
                          OnCountTabsForWindow on_counted);

  void ClearAllNodes();

  void ClearAllDivergentNodes();

  void ClearWindow(std::string_view window_tag);

  void ClearDivergentNodesForWindow(std::string_view window_tag,
                                    bool is_off_the_record);

  void ClearDivergenceWindow(std::string_view window_tag);

  void ClearNodesForWindowExcept(std::string_view window_tag,
                                 bool is_off_the_record,
                                 std::vector<StorageId> ids);

  void SetKey(std::string_view window_tag, std::vector<uint8_t> key);
  void RemoveKey(std::string_view window_tag);

#if defined(NDEBUG)
  void PrintAll();
#endif

 private:
  void OnDBReady(bool success);
  void OnWrite(bool success);

  void IncrementBoostCounter();
  void DecrementBoostCounter();
  void OnLoadDone(OnStorageLoadedData on_storage_loaded_data,
                  std::unique_ptr<StorageLoadedData> storage_loaded_data);

  const base::FilePath profile_path_;
  const bool support_off_the_record_data_;
  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;
  int boosted_priority_count_{0};

  // Use unique_ptr to allow for delayed destruction, as we want all pending
  // tasks to complete before destroying the object.
  std::unique_ptr<TabStateStorageDatabase> database_;

  base::WeakPtrFactory<TabStateStorageBackend> weak_ptr_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_BACKEND_H_
