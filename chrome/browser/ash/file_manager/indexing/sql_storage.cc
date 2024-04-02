// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/sql_storage.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "sql/error_delegate_util.h"
#include "sql/statement.h"

namespace file_manager {

enum class DbOperationStatus {
  kUnknown = 0,
  kOpenOk,
  kDirectoryCreateError,
  kOpenDbError,
  kTableInitError,

  kMaxValue = kTableInitError,
};

SqlStorage::SqlStorage(base::FilePath db_path, const std::string& uma_tag)
    : uma_tag_(uma_tag),
      db_path_(db_path),
      db_(sql::Database(sql::DatabaseOptions())),
      term_table_(&db_) {}

SqlStorage::~SqlStorage() {
  db_.reset_error_callback();
}

bool SqlStorage::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make sure we have the directory and open the database on it. Set histogram
  // tags, and error handlers.
  const base::FilePath db_dir = db_path_.DirName();
  if (!base::PathExists(db_dir) && !base::CreateDirectory(db_dir)) {
    LOG(ERROR) << "Failed to create a db directory " << db_dir;
    base::UmaHistogramEnumeration(uma_tag_,
                                  DbOperationStatus::kDirectoryCreateError);
    return false;
  }

  db_.set_histogram_tag(uma_tag_);

  if (!db_.Open(db_path_)) {
    LOG(ERROR) << "Failed to open " << db_path_;
    base::UmaHistogramEnumeration(uma_tag_, DbOperationStatus::kOpenDbError);
    return false;
  }
  // base::Unretained is safe as SqlStorage (this) owns the sql::Database (db_).
  // It thus always outlives it, as destroying this, requires db_ to be
  // destroyed first and thus guarantees that the error callback cannot be
  // invoked after db_ and this are destroyed.
  db_.set_error_callback(base::BindRepeating(&SqlStorage::OnErrorCallback,
                                             base::Unretained(this)));

  // Initialize all tables owned by SqlStorage.
  if (!term_table_.Init()) {
    LOG(ERROR) << "Failed to initialize term table";
    base::UmaHistogramEnumeration(uma_tag_, DbOperationStatus::kTableInitError);
    return false;
  }

  // Record successful operation and let the world know.
  base::UmaHistogramEnumeration(uma_tag_, DbOperationStatus::kOpenOk);
  return true;
}

bool SqlStorage::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_.Close();
  return true;
}

void SqlStorage::OnErrorCallback(int error, sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << "Database error: " << sql::ToSqliteResultCode(error);
  if (stmt) {
    LOG(ERROR) << "Database error statement: " << stmt->GetSQLStatement();
  }
  if (sql::IsErrorCatastrophic(error)) {
    LOG(ERROR) << "Database error is catastrophic.";
    db_.Poison();
  }
}

int64_t SqlStorage::GetTermId(const std::string& term_bytes, bool create) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return term_table_.GetTermId(term_bytes, create);
}

int64_t SqlStorage::DeleteTerm(const std::string& term_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return term_table_.DeleteTerm(term_bytes);
}

}  // namespace file_manager
