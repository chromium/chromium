// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"

#include "base/sequence_checker.h"
#include "sql/init_status.h"
#include "sql/transaction.h"

namespace ash::file_system_provider {

namespace {

static constexpr char kItemsCreateTableSql[] =
    // clang-format off
    "CREATE TABLE items ("
        "id INTEGER NOT NULL UNIQUE, "
        "atime INTEGER NOT NULL, "
        "version_tag TEXT NOT NULL, "
        "disk_path TEXT NOT NULL, "
        "fsp_path TEXT NOT NULL, PRIMARY "
        "KEY(id AUTOINCREMENT))";
// clang-format on

}  // namespace

ContextDatabase::ContextDatabase(const base::FilePath& db_path)
    : db_path_(db_path), db_({sql::DatabaseOptions{}}) {
  // Can be constructed on any sequence, the first call to `Initialize` should
  // be made on the blocking task runner.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

// The current database version number.
constexpr int ContextDatabase::kCurrentVersionNumber = 1;

// The oldest version that is still compatible with `kCurrentVersionNumber`.
constexpr int ContextDatabase::kCompatibleVersionNumber = 1;

bool ContextDatabase::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_.set_histogram_tag("FSPContextDatabase");

  DCHECK(!db_.is_open()) << "Database is already open";

  if (db_path_.empty() && !db_.OpenInMemory()) {
    LOG(ERROR) << "In memory database initialization failed";
    return false;
  } else if (!db_path_.empty() && !db_.Open(db_path_)) {
    LOG(ERROR) << "Initialization of '" << db_path_.value() << "' failed";
    Raze();
    return false;
  }

  sql::Transaction committer(&db_);
  if (!committer.Begin()) {
    LOG(ERROR) << "Can't start SQL transaction";
    return false;
  }

  if (!db_.DoesTableExist("items") && !db_.Execute(kItemsCreateTableSql)) {
    LOG(ERROR) << "Can't setup items table";
    Raze();
    return false;
  }

  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    Raze();
    return false;
  }

  return committer.Commit();
}

bool ContextDatabase::Raze() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  meta_table_.Reset();
  if (!db_.is_open()) {
    return true;
  }

  db_.Poison();
  return sql::Database::Delete(db_path_);
}

ContextDatabase::~ContextDatabase() = default;

}  // namespace ash::file_system_provider
