// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/term_table.h"

#include "base/logging.h"
#include "sql/statement.h"

namespace file_manager {

namespace {

#define TERM_TABLE "term_table"
#define TERM_ID "term_id"
#define TERM "term"
#define TERM_INDEX "term_index"

// The statement used to create the term table.
static constexpr char kCreateTermTableQuery[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS " TERM_TABLE "("
      TERM_ID " INTEGER PRIMARY KEY AUTOINCREMENT,"
      TERM " TEXT NOT NULL)";
// clang-format on

// The statement used to delete a term from the database by term ID.
static constexpr char kDeleteTermQuery[] =
    // clang-format off
    "DELETE FROM " TERM_TABLE " WHERE " TERM_ID "=?";
// clang-format on

// The statement used fetch the ID of the term.
static constexpr char kGetTermIdQuery[] =
    // clang-format off
    "SELECT " TERM_ID " FROM " TERM_TABLE " WHERE " TERM "=?";
// clang-format on

// The statement used fetch the ID of the term.
static constexpr char kGetTermValueQuery[] =
    // clang-format off
    "SELECT " TERM " FROM " TERM_TABLE " WHERE " TERM_ID "=?";
// clang-format on

// The statement used to insert a new term into the table.
static constexpr char kInsertTermQuery[] =
    // clang-format off
    "INSERT INTO " TERM_TABLE "(" TERM ") VALUES (?) RETURNING " TERM_ID;
// clang-format on

// The statement that creates an index on terms.
static constexpr char kCreateTermIndexQuery[] =
    // clang-format off
    "CREATE UNIQUE INDEX IF NOT EXISTS " TERM_INDEX " ON " TERM_TABLE
    "(" TERM ")";
// clang-format on

}  // namespace

TermTable::TermTable(sql::Database* db) : TextTable(db, "" TERM_TABLE "") {}
TermTable::~TermTable() = default;

int64_t TermTable::DeleteTerm(const std::string& term) {
  return DeleteValue(term);
}

int64_t TermTable::GetTerm(int64_t term_id, std::string* term) const {
  return GetValue(term_id, term);
}

int64_t TermTable::GetTermId(const std::string& term) const {
  return GetValueId(term);
}

int64_t TermTable::GetOrCreateTermId(const std::string& term) {
  return GetOrCreateValueId(term);
}

std::unique_ptr<sql::Statement> TermTable::MakeGetValueIdStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetTermIdQuery));
}

std::unique_ptr<sql::Statement> TermTable::MakeGetValueStatement() const {
  return std::make_unique<sql::Statement>(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetTermValueQuery));
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
