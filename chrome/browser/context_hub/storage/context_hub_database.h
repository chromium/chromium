// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_STORAGE_CONTEXT_HUB_DATABASE_H_
#define CHROME_BROWSER_CONTEXT_HUB_STORAGE_CONTEXT_HUB_DATABASE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/context_hub/storage/memory_bank_table.h"

namespace sql {
class Database;
}  // namespace sql

namespace context_hub {

// The central SQLite database manager for ContextHub.
class ContextHubDatabase {
 public:
  // Current database schema version.
  static constexpr int kCurrentVersionNumber = 1;

  ContextHubDatabase();
  ContextHubDatabase(const ContextHubDatabase&) = delete;
  ContextHubDatabase& operator=(const ContextHubDatabase&) = delete;
  ~ContextHubDatabase();

  // Initializes the database at `db_path`. Creates tables and performs schema
  // migrations if needed. Returns true on success.
  bool Init(const base::FilePath& db_path);

  // Operations on memory bank entries (delegates to MemoryBankTable):
  bool AddOrUpdateMemoryBankEntry(const MemoryBankEntry& entry);
  std::optional<MemoryBankEntry> GetMemoryBankEntry(int64_t id);
  std::vector<MemoryBankEntry> GetMemoryBankEntriesByIds(
      base::span<const int64_t> ids);
  std::vector<MemoryBankEntry> GetAllMemoryBankEntries();
  bool DeleteMemoryBankEntries(base::span<const int64_t> ids);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Migrates the database schema from `detected_user_version` to
  // `kCurrentVersionNumber`.
  bool MigrateOldVersionsAsNeeded(int detected_user_version);

  std::unique_ptr<sql::Database> db_;

  MemoryBankTable memory_bank_table_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_STORAGE_CONTEXT_HUB_DATABASE_H_
