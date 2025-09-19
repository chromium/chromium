// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"

namespace sql {
class Database;
class MetaTable;
}  // namespace sql

namespace tabs {

// Represents a row in the node table, to allow returning many rows of data.
// Each row may be a tab or parent collection.
struct NodeState {
  int id;
  int type;
  std::string payload;
  std::string children;
};

// This class is responsible for all database operations.
class TabStateStorageDatabase {
 public:
  explicit TabStateStorageDatabase(const base::FilePath& profile_path);
  ~TabStateStorageDatabase();
  TabStateStorageDatabase(const TabStateStorageDatabase&) = delete;
  TabStateStorageDatabase& operator=(const TabStateStorageDatabase&) = delete;

  // Initializes the database.
  bool Initialize();

  // Saves a node to the database.
  bool SaveNode(int id, int type, std::string payload, std::string children);

  // Loads all nodes from the database.
  std::vector<NodeState> LoadAllNodes();

 private:
  base::FilePath profile_path_;
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<sql::MetaTable> meta_table_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_
