// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/documents_table.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/local_image_search/sql_database.h"
#include "sql/statement.h"

namespace app_list {

// static
bool DocumentsTable::Create(SqlDatabase* db) {
  static constexpr char kQuery[] =
      // clang-format off
      "CREATE TABLE documents("
          "document_id INTEGER PRIMARY KEY,"
          "directory_path TEXT NOT NULL,"
          "file_name TEXT NOT NULL,"
          "last_modified_time INTEGER NOT NULL,"
          "file_size INTEGER NOT NULL,"
          "UNIQUE (directory_path, file_name))";
  // clang-format on

  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement || !statement->Run()) {
    LOG(ERROR) << "Failed to create documents.";
    return false;
  }

  static constexpr char kQuery1[] =
      "CREATE INDEX idx_documents_filepath "
      "ON documents(directory_path, file_name)";

  std::unique_ptr<sql::Statement> statement1 =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery1);
  if (!statement1 || !statement1->Run()) {
    LOG(ERROR) << "Couldn't execute the statement";
    return false;
  }

  return true;
}

// static
bool DocumentsTable::Drop(SqlDatabase* db) {
  static constexpr char kQuery[] = "DROP TABLE IF EXISTS documents";

  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement || !statement->Run()) {
    LOG(ERROR) << "Couldn't execute the statement";
    return false;
  }

  return true;
}

// static
bool DocumentsTable::InsertOrIgnore(SqlDatabase* db,
                                    const base::FilePath& file_path,
                                    const base::Time& last_modified_time,
                                    int64_t file_size) {
  static constexpr char kQuery[] =
      // clang-format off
      "INSERT OR IGNORE INTO documents"
        "(directory_path, file_name, last_modified_time, file_size) "
        "VALUES(?,?,?,?)";
  // clang-format on

  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    LOG(ERROR) << "Couldn't create the statement";
    return false;
  }

  // Safe on ChromeOS.
  statement->BindString(0, file_path.DirName().AsUTF8Unsafe());
  statement->BindString(1, file_path.BaseName().AsUTF8Unsafe());
  statement->BindTime(2, last_modified_time);
  statement->BindInt64(3, file_size);
  if (!statement->Run()) {
    LOG(ERROR) << "Couldn't execute the statement";
    return false;
  }

  return true;
}

// static
bool DocumentsTable::GetDocumentId(SqlDatabase* db,
                                   const base::FilePath& file_path,
                                   int64_t& document_id) {
  DVLOG(2) << "GetDocumentId " << file_path;
  static constexpr char kQuery[] =
      "SELECT document_id FROM documents WHERE directory_path=? AND "
      "file_name=?";

  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    LOG(ERROR) << "Couldn't create the statement";
    return false;
  }

  statement->BindString(0, file_path.DirName().AsUTF8Unsafe());
  statement->BindString(1, file_path.BaseName().AsUTF8Unsafe());
  if (!statement->Step()) {
    LOG(ERROR) << "Couldn't execute the statement";
    return false;
  }
  document_id = statement->ColumnInt64(0);
  DVLOG(2) << "document_id " << document_id;
  return true;
}

// static
bool DocumentsTable::Remove(SqlDatabase* db, const base::FilePath& file_path) {
  static constexpr char kQuery[] =
      "DELETE FROM documents WHERE directory_path=? AND file_name=?";

  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    LOG(ERROR) << "Couldn't create the statement";
    return false;
  }

  statement->BindString(0, file_path.DirName().AsUTF8Unsafe());
  statement->BindString(1, file_path.BaseName().AsUTF8Unsafe());
  if (!statement->Run()) {
    LOG(ERROR) << "Couldn't execute the statement";
    return false;
  }
  return true;
}

}  // namespace app_list
