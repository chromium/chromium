// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_STORAGE_MEMORY_BANK_TABLE_H_
#define CHROME_BROWSER_CONTEXT_HUB_STORAGE_MEMORY_BANK_TABLE_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"

namespace sql {
class Database;
}  // namespace sql

namespace context_hub {

// This class manages the table storing memory bank entries within the
// SQLite database passed to initialization.
//
// Schema:
// -----------------------------------------------------------------------------
// memory_bank_entries
//
//   id                                 INTEGER PRIMARY KEY Auto-incremented by
//                                      SQLite, uniquely identifies each entry.
//   type                               INTEGER NOT NULL (0: kTab,
//                                      1: kTextSelection)
//   timestamp                          INTEGER NOT NULL The timestamp of the
//                                      memory bank entry, stored as
//                                      microseconds since the Windows epoch.
//   url                                TEXT NOT NULL The URL of the memory bank
//                                      entry.
//   tab_title                          TEXT NOT NULL The title of the tab
//                                      associated with the memory bank entry.
//   selected_text                      TEXT The selected text from the page.
//   tags                               TEXT (JSON-serialized string of tags)
// -----------------------------------------------------------------------------
class MemoryBankTable {
 public:
  MemoryBankTable();
  MemoryBankTable(const MemoryBankTable&) = delete;
  MemoryBankTable& operator=(const MemoryBankTable&) = delete;
  ~MemoryBankTable();

  // Initializes the table with the given SQLite database. Must be called
  // before any other methods. Returns true on success.
  bool Init(sql::Database* db);

  // Creates the memory_bank_entries table and its indexes at database
  // version 1. Should only be called when initializing or migrating a database
  // from a clean state.
  bool MigrateFromCleanStateToVersion1();

  // Inserts or replaces a record in memory_bank_entries. Returns true on
  // success.
  bool AddOrUpdateEntry(const MemoryBankEntry& entry);

  // Retrieves a single entry by ID. Returns std::nullopt if not found.
  std::optional<MemoryBankEntry> GetEntry(int64_t id);

  // Retrieves all entries from memory_bank_entries ordered by timestamp DESC.
  std::vector<MemoryBankEntry> GetAllEntries();

  // Deletes records by IDs. Returns true on success.
  bool DeleteEntries(base::span<const int64_t> ids);

 private:
  // Owned by ContextHubDatabase, outlives this class.
  raw_ptr<sql::Database> db_ = nullptr;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_STORAGE_MEMORY_BANK_TABLE_H_
