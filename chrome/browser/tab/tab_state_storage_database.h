// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/tab/tab_storage_type.h"

namespace sql {
class Database;
class MetaTable;
class Transaction;
}  // namespace sql

namespace tabs {

// Represents a row in the node table, to allow returning many rows of data.
// Each row may be a tab or parent collection.
struct NodeState {
  int id;
  TabStorageType type;
  std::string payload;
  std::string children;
};

// This class is responsible for all database operations.
class TabStateStorageDatabase {
 public:
  // Holds an sql::Transaction. Used as a key for database updates.
  class Transaction {
   public:
    explicit Transaction(std::unique_ptr<sql::Transaction> transaction);
    ~Transaction();

    // Starts a transaction. Returns false in the case of failures.
    bool Begin();

    // Rolls back the transaction.
    void Rollback();

    // Commits the transaction. Returns false in the case of failures.
    bool Commit();

    // Returns true if the transaction is still open.
    bool IsOpen();

   private:
    std::unique_ptr<sql::Transaction> transaction_;
  };

  explicit TabStateStorageDatabase(const base::FilePath& profile_path);
  ~TabStateStorageDatabase();
  TabStateStorageDatabase(const TabStateStorageDatabase&) = delete;
  TabStateStorageDatabase& operator=(const TabStateStorageDatabase&) = delete;

  // Initializes the database.
  bool Initialize();

  // Saves a node to the database.
  bool SaveNode(Transaction* transaction,
                int id,
                TabStorageType type,
                std::string payload,
                std::string children);

  // Saves the children of a node to the database.
  // This will silently fail if the node does not already exist.
  bool SaveNodeChildren(Transaction* transaction, int id, std::string children);

  // Removes a node from the database.
  // This will silently fail if the node does not already exist.
  bool RemoveNode(Transaction* transaction, int id);

  // Creates a transaction.
  std::unique_ptr<Transaction> CreateTransaction();

  // Loads all nodes from the database.
  std::vector<NodeState> LoadAllNodes();

 private:
  base::FilePath profile_path_;
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<sql::MetaTable> meta_table_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_
