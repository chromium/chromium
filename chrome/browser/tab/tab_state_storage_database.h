// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_

#include <cstdint>
#include <memory>
#include <string>
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
  NodeState(int id,
            TabStorageType type,
            std::vector<uint8_t> payload,
            std::vector<uint8_t> children);
  ~NodeState();

  NodeState(const NodeState&) = delete;
  NodeState& operator=(const NodeState&) = delete;

  NodeState(NodeState&&) noexcept;
  NodeState& operator=(NodeState&&) noexcept;

  int id;
  TabStorageType type;
  std::vector<uint8_t> payload;
  std::vector<uint8_t> children;
};

// This class is responsible for all database operations.
class TabStateStorageDatabase {
 public:
  // Represents a transaction. Used as a key for database updates, and must be
  // returned to commit the transaction.
  class OpenTransaction {
   public:
    ~OpenTransaction();
    OpenTransaction(const OpenTransaction&) = delete;
    OpenTransaction& operator=(const OpenTransaction&) = delete;

    // Marks the transaction as failed. Rolls back the transaction once
    // returned.
    void MarkFailed();

    // Returns whether the transaction has failed.
    bool HasFailed();

    // Returns whether the transaction is valid.
    static bool IsValid(OpenTransaction* transaction);

   private:
    friend TabStateStorageDatabase;
    explicit OpenTransaction(std::unique_ptr<sql::Transaction> transaction);

    sql::Transaction* GetTransaction();

    std::unique_ptr<sql::Transaction> transaction_;
    bool mark_failed_ = false;
  };

  explicit TabStateStorageDatabase(const base::FilePath& profile_path);
  ~TabStateStorageDatabase();
  TabStateStorageDatabase(const TabStateStorageDatabase&) = delete;
  TabStateStorageDatabase& operator=(const TabStateStorageDatabase&) = delete;

  // Initializes the database.
  bool Initialize();

  // Saves a node to the database.
  bool SaveNode(OpenTransaction* transaction,
                int id,
                std::string window_tag,
                bool is_off_the_record,
                TabStorageType type,
                std::vector<uint8_t> payload,
                std::vector<uint8_t> children);

  // Saves a node payload to the database.
  // This will silently fail if the node does not already exist.
  bool SaveNodePayload(OpenTransaction* transaction,
                       int id,
                       std::vector<uint8_t> payload);

  // Saves the children of a node to the database.
  // This will silently fail if the node does not already exist.
  bool SaveNodeChildren(OpenTransaction* transaction,
                        int id,
                        std::vector<uint8_t> children);

  // Removes a node from the database.
  // This will silently fail if the node does not already exist.
  bool RemoveNode(OpenTransaction* transaction, int id);

  // Creates an open transaction.
  OpenTransaction* CreateTransaction();

  // Closes the existing transaction.
  bool CloseTransaction(OpenTransaction* transaction);

  // Loads all nodes from the database.
  std::vector<NodeState> LoadAllNodes(std::string window_tag,
                                      bool is_off_the_record);

  // Clears all nodes from the database.
  void ClearAllNodes();

 private:
  std::unique_ptr<OpenTransaction> open_transaction_;
  base::FilePath profile_path_;
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<sql::MetaTable> meta_table_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_
