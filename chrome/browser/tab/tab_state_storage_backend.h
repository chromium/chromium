// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_BACKEND_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_BACKEND_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/proto_table_manager.h"
#include "sql/database.h"
#include "sql/meta_table.h"

namespace tabs {

// TODO(https://crbug.com/427254826): Split this class into pieces, each working
// on a dedicated thread.
class TabStateStorageBackend {
 public:
  explicit TabStateStorageBackend(const base::FilePath& profile_path);
  TabStateStorageBackend(const TabStateStorageBackend&) = delete;
  TabStateStorageBackend& operator=(const TabStateStorageBackend&) = delete;
  ~TabStateStorageBackend();

  void Initialize();

  void SaveTabState(int id,
                    int parent,
                    std::string position,
                    tabs_pb::TabState tab_state);

  void LoadAllTabStates(
      base::OnceCallback<void(std::vector<tabs_pb::TabState>)> callback);

 private:
  void OnDBReady(bool success);
  void OnWrite(bool success);
  std::vector<tabs_pb::TabState> ReadAllTabs();
  void OnAllTabsRead(
      base::OnceCallback<void(std::vector<tabs_pb::TabState>)> callback,
      std::vector<tabs_pb::TabState> result);

  base::FilePath profile_path_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<sql::MetaTable> meta_table_;

  base::WeakPtrFactory<TabStateStorageBackend> weak_ptr_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_BACKEND_H_
