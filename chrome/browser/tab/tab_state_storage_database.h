// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/types/pass_key.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "chrome/browser/tab/tab_storage_type.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"

namespace tabs {

// This class is responsible for all database operations.
class TabStateStorageDatabase {
 public:
  // Represents a transaction. Used as a key for database updates, and must be
  // returned to commit the transaction.
  class OpenTransaction {
   public:
    OpenTransaction(sql::Database* db, base::PassKey<TabStateStorageDatabase>);

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

    // Returns the underlying transaction.
    sql::Transaction* GetTransaction(base::PassKey<TabStateStorageDatabase>);

   private:
    sql::Transaction transaction_;
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
                StorageId id,
                std::string window_tag,
                bool is_off_the_record,
                TabStorageType type,
                std::vector<uint8_t> payload,
                std::vector<uint8_t> children);

  // Saves a node payload to the database.
  // This will silently fail if the node does not already exist.
  bool SaveNodePayload(OpenTransaction* transaction,
                       StorageId id,
                       std::vector<uint8_t> payload);

  // Saves the children of a node to the database.
  // This will silently fail if the node does not already exist.
  bool SaveNodeChildren(OpenTransaction* transaction,
                        StorageId id,
                        std::vector<uint8_t> children);

  // Removes a node from the database.
  // This will silently fail if the node does not already exist.
  bool RemoveNode(OpenTransaction* transaction, StorageId id);

  // Creates an open transaction.
  OpenTransaction* CreateTransaction();

  // Closes the existing transaction.
  bool CloseTransaction(OpenTransaction* transaction);

  // Loads all nodes from the database.
  std::unique_ptr<StorageLoadedData> LoadAllNodes(
      const std::string& window_tag,
      bool is_off_the_record,
      std::unique_ptr<StorageLoadedData::Builder> builder);

  // Clears all nodes from the database.
  void ClearAllNodes();

  // Clears all nodes for a given window from the database.
  void ClearWindow(const std::string& window_tag);

 private:
  base::FilePath profile_path_;
  sql::Database db_;
  sql::MetaTable meta_table_;
  std::optional<OpenTransaction> open_transaction_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_
