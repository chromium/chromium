// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/annotations_table.h"

#include <memory>

#include "base/logging.h"
#include "chrome/browser/ash/app_list/search/local_image_search/sql_database.h"
#include "sql/statement.h"

namespace app_list {

// static
bool AnnotationsTable::Create(SqlDatabase* db) {
  static constexpr char kQuery[] =
      // clang-format off
      "CREATE TABLE annotations("
          "term_id INTEGER PRIMARY KEY,"
          "term TEXT UNIQUE)";
  // clang-format on

  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement || !statement->Run()) {
    LOG(ERROR) << "Couldn't execute the statement";
    return false;
  }

  static constexpr char kQuery1[] =
      "CREATE INDEX idx_annotations_term ON annotations(term)";

  std::unique_ptr<sql::Statement> statement1 =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery1);
  if (!statement1 || !statement1->Run()) {
    LOG(ERROR) << "Couldn't execute the statement";
    return false;
  }

  return true;
}

// static
bool AnnotationsTable::Drop(SqlDatabase* db) {
  static constexpr char kQuery[] = "DROP TABLE IF EXISTS annotations";

  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement || !statement->Run()) {
    LOG(ERROR) << "Couldn't execute the statement";
    return false;
  }

  return true;
}

// static
bool AnnotationsTable::InsertOrIgnore(SqlDatabase* db,
                                      const std::string& term) {
  static constexpr char kQuery[] =
      "INSERT OR IGNORE INTO annotations(term) VALUES(?)";

  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    LOG(ERROR) << "Couldn't create the statement";
    return false;
  }

  statement->BindString(0, term);
  if (!statement->Run()) {
    LOG(ERROR) << "Couldn't execute the statement";
    return false;
  }
  return true;
}

// static
bool AnnotationsTable::GetTermId(SqlDatabase* db,
                                 const std::string& term,
                                 int64_t& term_id) {
  DVLOG(2) << "GetTermId " << term;
  static constexpr char kSearchQuery[] =
      "SELECT term_id FROM annotations WHERE term=?";

  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kSearchQuery);
  if (!statement) {
    LOG(ERROR) << "Failed to create documents.";
    return false;
  }
  statement->BindString(0, term);
  if (!statement->Step()) {
    LOG(ERROR) << "Couldn't execute the statement";
    return false;
  }

  term_id = statement->ColumnInt64(0);
  DVLOG(2) << "term_id " << term_id;
  return true;
}

// static
bool AnnotationsTable::Prune(SqlDatabase* db) {
  static constexpr char kQuery[] =
      // clang-format off
      "DELETE FROM annotations "
      "WHERE term_id NOT IN (SELECT term_id FROM inverted_index)";
  // clang-format on

  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement || !statement->Run()) {
    LOG(ERROR) << "Couldn't execute the statement";
    return false;
  }

  return true;
}

}  // namespace app_list
