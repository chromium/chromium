// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
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

  TabStateStorageDatabase(const base::FilePath& profile_path,
                          bool support_off_the_record_data);
  ~TabStateStorageDatabase();
  TabStateStorageDatabase(const TabStateStorageDatabase&) = delete;
  TabStateStorageDatabase& operator=(const TabStateStorageDatabase&) = delete;

  // Initializes the database.
  bool Initialize();

  // Saves a node to the database.
  bool SaveNode(OpenTransaction* transaction,
                StorageId id,
                std::string_view window_tag,
                bool is_off_the_record,
                TabStorageType type,
                std::vector<uint8_t> payload,
                std::vector<uint8_t> children);

  // Saves a node payload to the database.
  // This will silently fail if the node does not already exist.
  bool SaveNodePayload(OpenTransaction* transaction,
                       StorageId id,
                       std::string_view window_tag,
                       bool is_off_the_record,
                       std::vector<uint8_t> payload);

  // Saves the children of a node to the database.
  // This will silently fail if the node does not already exist.
  bool SaveNodeChildren(OpenTransaction* transaction,
                        StorageId id,
                        std::vector<uint8_t> children);

  // Inserts or updates a divergent node.
  bool SaveDivergentNode(OpenTransaction* transaction,
                         StorageId id,
                         std::string_view window_tag,
                         bool is_off_the_record,
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
      std::string_view window_tag,
      bool is_off_the_record,
      std::unique_ptr<StorageLoadedData::Builder> builder);

  // Clears all nodes from the database.
  void ClearAllNodes();

  // Clears all divergent nodes from the database.
  void ClearAllDivergentNodes();

  // Clears all nodes for a given window from the database.
  void ClearWindow(std::string_view window_tag);

  // Clears all divergent nodes for a given window from the database.
  void ClearDivergentNodesForWindow(std::string_view window_tag,
                                    bool is_off_the_record);

  // Clears a divergence window from the database.
  void ClearDivergenceWindow(std::string_view window_tag);

  // Clears all nodes for a given window from the database except for the
  // provided storage IDs.
  bool ClearNodesForWindowExcept(std::string_view window_tag,
                                 bool is_off_the_record,
                                 const std::vector<StorageId>& ids);

  // Counts the number of tabs for a given window.
  int CountTabsForWindow(std::string_view window_tag, bool is_off_the_record);

  // Sets the key to seal OTR payloads with. The window tag is moved
  // internally and this is always called in a posted callback hence
  // the use of std::string.
  void SetKey(std::string window_tag, std::vector<uint8_t> key);

  // Remove key for OTR sealing from a given window.
  void RemoveKey(std::string_view window_tag);

#if defined(NDEBUG)
  // Dumps the entire state of the database to the log for debugging. Do not use
  // in production.
  //
  // Because `StorageId` tokens are randomly generated and difficult to visually
  // parse, this method maps them to sequential, temporary integers
  // (e.g., 1, 2, 3...) for the duration of the dump.
  //
  // The output consists of:
  // 1. A list of all nodes, using the temporary integers for `id` and
  // `children`.
  // 2. A legend mapping each temporary integer back to its original `StorageId`
  // token.
  void PrintAll();
#endif

 private:
  std::optional<std::vector<uint8_t>> Seal(StorageId id,
                                           std::string_view window_tag,
                                           base::span<const uint8_t> payload);
  std::optional<std::vector<uint8_t>> Open(StorageId storage_id,
                                           std::string_view window_tag,
                                           base::span<const uint8_t> payload);

  const base::FilePath profile_path_;
  const bool support_off_the_record_data_;
  sql::Database db_;
  sql::MetaTable meta_table_;
  std::optional<OpenTransaction> open_transaction_;
  int open_transaction_count_ = 0;

  // A map of window tags to their associated keys for OTR payloads.
  absl::flat_hash_map<std::string, std::vector<uint8_t>> keys_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_DATABASE_H_
