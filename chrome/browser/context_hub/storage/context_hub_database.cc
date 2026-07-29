// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/storage/context_hub_database.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/context_hub/features.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace context_hub {

ContextHubDatabase::ContextHubDatabase() = default;

ContextHubDatabase::~ContextHubDatabase() = default;

bool ContextHubDatabase::Init(const base::FilePath& db_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_ = std::make_unique<sql::Database>(
      sql::DatabaseOptions()
#if BUILDFLAG(IS_WIN)
          .set_exclusive_database_file_lock(true)
#endif
          .set_wal_mode(true),
      sql::Database::Tag("ContextHub"));

  int captured_error = 0;
  db_->set_error_callback(
      base::BindRepeating([](int* out_error, int error,
                             sql::Statement* stmt) { *out_error = error; },
                          &captured_error));

  bool open_success = db_->Open(db_path);
  db_->reset_error_callback();

  if (!open_success) {
    // If the error is kNotADatabase, attempt to delete the file and re-open
    // the database.
    if (captured_error ==
        std::to_underlying(sql::SqliteResultCode::kNotADatabase)) {
      if (!db_->CloseAndDelete()) {
        return false;
      }
      if (!db_->Open(db_path)) {
        return false;
      }
    } else {
      return false;
    }
  }

  int detected_user_version = 0;
  // Check the user-version (https://sqlite.org/pragma.html#pragma_user_version)
  // to see if there has been a schema change since the last time this database
  // was modified.
  if (sql::Statement get_user_version_stm(
          db_->GetUniqueStatement("PRAGMA user_version"));
      get_user_version_stm.is_valid() && get_user_version_stm.Step()) {
    detected_user_version = get_user_version_stm.ColumnInt(0);
  } else {
    return false;
  }

  if (!memory_bank_table_.Init(db_.get())) {
    return false;
  }

  if (detected_user_version == kCurrentVersionNumber) {
    return true;
  }

  // If the database is newer than the current version, leave it unmodified.
  // This allows the data to be used again once the browser is updated.
  if (detected_user_version > kCurrentVersionNumber) {
    return false;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  if (!MigrateOldVersionsAsNeeded(detected_user_version)) {
    return false;
  }

  return transaction.Commit();
}

bool ContextHubDatabase::AddOrUpdateMemoryBankEntry(
    const MemoryBankEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_ || !db_->is_open()) {
    return false;
  }

  size_t max_entries = features::kMaxMemoryBankEntries.Get();
  // If adding a new entry and the max entries limit is reached, reject.
  if (entry.id == 0 && memory_bank_table_.GetEntryCount() >= max_entries) {
    return false;
  }

  return memory_bank_table_.AddOrUpdateEntry(entry);
}

std::optional<MemoryBankEntry> ContextHubDatabase::GetMemoryBankEntry(
    int64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_ || !db_->is_open()) {
    return std::nullopt;
  }
  return memory_bank_table_.GetEntry(id);
}

std::vector<MemoryBankEntry> ContextHubDatabase::GetMemoryBankEntriesByIds(
    base::span<const int64_t> ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_ || !db_->is_open()) {
    return {};
  }
  return memory_bank_table_.GetEntriesByIds(ids);
}

std::vector<MemoryBankEntry> ContextHubDatabase::GetAllMemoryBankEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_ || !db_->is_open()) {
    return {};
  }
  return memory_bank_table_.GetAllEntries();
}

bool ContextHubDatabase::DeleteMemoryBankEntries(
    base::span<const int64_t> ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_ || !db_->is_open()) {
    return false;
  }
  return memory_bank_table_.DeleteEntries(ids);
}

bool ContextHubDatabase::MigrateOldVersionsAsNeeded(int detected_user_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_ || !db_->is_open()) {
    return false;
  }

  if (detected_user_version == 0) {
    if (!memory_bank_table_.MigrateFromCleanStateToVersion1()) {
      return false;
    }
    detected_user_version++;
  }

  return db_->Execute(base::StrCat(
      {"PRAGMA user_version=", base::NumberToString(kCurrentVersionNumber)}));
}

}  // namespace context_hub
