// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/term_table.h"

#include "base/logging.h"
#include "sql/statement.h"

namespace file_manager {

namespace {

// The statement used to create the term table.
static constexpr char kCreateTermTableQuery[] =
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

// The statement that creates an index on terms.
static constexpr char kCreateTermIndexQuery[] =
    // clang-format off
    "CREATE UNIQUE INDEX IF NOT EXISTS term_index ON term_table(term)";
// clang-format on

}  // namespace

TermTable::TermTable(sql::Database* db) : TextTable(db, "term_table") {}
TermTable::~TermTable() = default;

int64_t TermTable::DeleteTerm(const std::string& term) {
  return DeleteValue(term);
}

int64_t TermTable::GetTermId(const std::string& term, bool create) {
  if (create) {
    return GetOrCreateValueId(term);
  }
  return GetValueId(term);
}

std::unique_ptr<sql::Statement> TermTable::MakeGetStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetTermIdQuery));
}

std::unique_ptr<sql::Statement> TermTable::MakeInsertStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertTermQuery));
}

std::unique_ptr<sql::Statement> TermTable::MakeDeleteStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteTermQuery));
}

std::unique_ptr<sql::Statement> TermTable::MakeCreateTableStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateTermTableQuery));
}

std::unique_ptr<sql::Statement> TermTable::MakeCreateIndexStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateTermIndexQuery));
}
}  // namespace file_manager
