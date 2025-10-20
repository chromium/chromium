// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_BACKEND_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_BACKEND_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/tab/tab_state_storage_database.h"
#include "chrome/browser/tab/tab_state_storage_updater.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "chrome/browser/tab/tab_storage_type.h"

namespace tabs {

// Backend for TabStateStorage, responsible for coordinating with the storage
// layer.
class TabStateStorageBackend {
 public:
  explicit TabStateStorageBackend(const base::FilePath& profile_path);
  TabStateStorageBackend(const TabStateStorageBackend&) = delete;
  TabStateStorageBackend& operator=(const TabStateStorageBackend&) = delete;
  ~TabStateStorageBackend();

  void Initialize();

  // Performs an atomic database update.
  void Update(std::unique_ptr<TabStateStorageUpdater> updater);

  void LoadAllNodes(base::OnceCallback<void(std::vector<NodeState>)> callback);

 private:
  void OnDBReady(bool success);
  void OnWrite(bool success);
  void OnAllTabsRead(base::OnceCallback<void(std::vector<NodeState>)> callback,
                     std::vector<NodeState> result);

  const base::FilePath profile_path_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  std::unique_ptr<TabStateStorageDatabase> database_;

  base::WeakPtrFactory<TabStateStorageBackend> weak_ptr_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_BACKEND_H_
