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
      token_table_(&db_),
      term_table_(&db_),
      url_table_(&db_),
      file_info_table_(&db_),
      posting_list_table_(&db_) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SqlStorage::~SqlStorage() {
  db_.Close();
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
  if (!token_table_.Init()) {
    LOG(ERROR) << "Failed to initialize token_table";
    base::UmaHistogramEnumeration(uma_tag_, DbOperationStatus::kTableInitError);
    return false;
  }
  if (!term_table_.Init()) {
    LOG(ERROR) << "Failed to initialize term_table";
    base::UmaHistogramEnumeration(uma_tag_, DbOperationStatus::kTableInitError);
    return false;
  }
  if (!url_table_.Init()) {
    LOG(ERROR) << "Failed to initialize url_table";
    base::UmaHistogramEnumeration(uma_tag_, DbOperationStatus::kTableInitError);
    return false;
  }
  if (!file_info_table_.Init()) {
    LOG(ERROR) << "Failed to initialize file_info_table";
    base::UmaHistogramEnumeration(uma_tag_, DbOperationStatus::kTableInitError);
    return false;
  }
  if (!posting_list_table_.Init()) {
    LOG(ERROR) << "Failed to initialize file_info_table";
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

void SqlStorage::AddTermIdsForUrl(const std::set<int64_t>& term_ids,
                                  int64_t url_id) {
  for (const int64_t term_id : term_ids) {
    AddToPostingList(term_id, url_id);
  }
}

void SqlStorage::DeleteTermIdsForUrl(const std::set<int64_t>& term_ids,
                                     int64_t url_id) {
  for (const int64_t term_id : term_ids) {
    DeleteFromPostingList(term_id, url_id);
  }
}

const std::set<int64_t> SqlStorage::GetUrlIdsForTermId(int64_t term_id) const {
  return posting_list_table_.GetUrlIdsForTerm(term_id);
}

const std::set<int64_t> SqlStorage::GetTermIdsForUrl(int64_t url_id) const {
  return posting_list_table_.GetTermIdsForUrl(url_id);
}

int32_t SqlStorage::AddToPostingList(int64_t term_id, int64_t url_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(term_id != -1);
  DCHECK(url_id != -1);
  return posting_list_table_.AddToPostingList(term_id, url_id);
}

int32_t SqlStorage::DeleteFromPostingList(int64_t term_id, int64_t url_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(term_id != -1);
  DCHECK(url_id != -1);
  return posting_list_table_.DeleteFromPostingList(term_id, url_id);
}

int64_t SqlStorage::GetTokenId(const std::string& term_bytes) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return token_table_.GetTokenId(term_bytes);
}

int64_t SqlStorage::GetOrCreateTokenId(const std::string& term_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return token_table_.GetOrCreateTokenId(term_bytes);
}

int64_t SqlStorage::GetTermId(const Term& term) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t term_id = GetTokenId(term.token_bytes());
  if (term_id == -1) {
    return -1;
  }
  return term_table_.GetTermId(term.field(), term_id);
}

int64_t SqlStorage::GetOrCreateTermId(const Term& term) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t term_id = GetTermId(term);
  if (term_id != -1) {
    return term_id;
  }
  int64_t token_id = GetOrCreateTokenId(term.token_bytes());
  return term_table_.GetOrCreateTermId(term.field(), token_id);
}

int64_t SqlStorage::GetOrCreateUrlId(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!url.is_valid()) {
    return -1;
  }
  return url_table_.GetOrCreateUrlId(url);
}

int64_t SqlStorage::GetUrlId(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!url.is_valid()) {
    return -1;
  }
  return url_table_.GetUrlId(url);
}

int64_t SqlStorage::DeleteUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_table_.DeleteUrl(url);
}

int64_t SqlStorage::PutFileInfo(const FileInfo& file_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t url_id = url_table_.GetOrCreateUrlId(file_info.file_url);
  if (url_id == -1) {
    return -1;
  }
  return file_info_table_.PutFileInfo(url_id, file_info);
}

int64_t SqlStorage::GetFileInfo(int64_t url_id, FileInfo* file_info) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (url_id == -1) {
    return -1;
  }
  std::string url_spec;
  if (url_table_.GetUrlSpec(url_id, &url_spec) == -1) {
    return -1;
  }
  GURL url(url_spec);
  if (!url.is_valid()) {
    return -1;
  }
  int64_t gotten_url_id = file_info_table_.GetFileInfo(url_id, file_info);
  if (gotten_url_id == -1) {
    return -1;
  }
  DCHECK(gotten_url_id == url_id);
  file_info->file_url = url;
  return url_id;
}

int64_t SqlStorage::DeleteFileInfo(int64_t url_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (url_id == -1) {
    return -1;
  }
  return file_info_table_.DeleteFileInfo(url_id);
}

}  // namespace file_manager
