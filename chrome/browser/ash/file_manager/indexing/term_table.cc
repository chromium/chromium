// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/term_table.h"

#include "base/logging.h"
#include "sql/statement.h"

namespace file_manager {

namespace {

// The statement used to create the term table.
static constexpr char kCreateTableQuery[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS term_table("
        "term_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "term TEXT NOT NULL)";
// clang-format on

// The statement used to delete a term from the database by term ID.
static constexpr char kDeleteTermQuery[] =
    // clang-format off
    "DELETE FROM term_table WHERE term_id = ?";
// clang-format on

// The statement used fetch the ID of the term.
static constexpr char kGetTermIdQuery[] =
    // clang-format off
    "SELECT term_id FROM term_table WHERE term = ?";
// clang-format on

// The statement used to insert a new term into the table.
static constexpr char kInsertTermQuery[] =
    // clang-format off
     "INSERT INTO term_table(term) VALUES (?) RETURNING term_id";
// clang-format on

}  // namespace

TermTable::TermTable(sql::Database* db) : db_(db) {}
TermTable::~TermTable() = default;

bool TermTable::Init() {
  if (!db_->is_open()) {
    LOG(WARNING) << "Unable to initialized TermTable due to closed db";
    return false;
  }
  sql::Statement create_table(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateTableQuery));
  if (!create_table.is_valid()) {
    DLOG(ERROR) << "Failed to crate the term table " << kCreateTableQuery;
    return false;
  }
  if (!create_table.Run()) {
    LOG(ERROR) << "Failed to create the term table";
    return false;
  }
  // TODO(b:327535200): Create a unique index on the term column.
  return true;
}

int64_t TermTable::DeleteTerm(const std::string& term) {
  int64_t term_id = GetTermId(term, false);
  if (term_id < 0) {
    return term_id;
  }

  sql::Statement delete_term_by_id(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteTermQuery));
  if (!delete_term_by_id.is_valid()) {
    DLOG(ERROR) << "Invalid delete statement: \"" << kDeleteTermQuery << "\"";
    return -1;
  }
  delete_term_by_id.BindInt64(0, term_id);
  if (!delete_term_by_id.Run()) {
    LOG(ERROR) << "Failed to delete terms matching " << term;
    return -1;
  }
  return term_id;
}

int64_t TermTable::GetTermId(const std::string& term, bool create) {
  sql::Statement get_term_id(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetTermIdQuery));
  get_term_id.BindString(0, term);
  if (!get_term_id.is_valid()) {
    DLOG(ERROR) << "Invalid get term ID statement: \"" << kGetTermIdQuery
                << "\"";
    return -1;
  }
  if (get_term_id.Step()) {
    return get_term_id.ColumnInt64(0);
  }
  if (!create) {
    return -1;
  }
  sql::Statement insert_term(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertTermQuery));
  if (!insert_term.is_valid()) {
    DLOG(ERROR) << "Invalid insert term statement \"" << kInsertTermQuery
                << "\"";
    return -1;
  }
  insert_term.BindString(0, term);
  if (insert_term.Step()) {
    return insert_term.ColumnInt64(0);
  }
  LOG(ERROR) << "Failed to insert term" << term;
  return -1;
}

}  // namespace file_manager
