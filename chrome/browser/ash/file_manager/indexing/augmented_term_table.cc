// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/augmented_term_table.h"

#include "sql/statement.h"

namespace file_manager {

namespace {

#define TABLE_NAME "augmented_term_table"

// The statement used to create the augmented_term table.
static constexpr char kCreateAugmentedTermTableQuery[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS " TABLE_NAME "("
      "augmented_term_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "field_name TEXT NOT NULL,"
      "term_id INTEGER NOT NULL REFERENCES term_table(term_id),"
      "UNIQUE(field_name, term_id))";
// clang-format on

// The statement used to insert a new augmented term into the table.
static constexpr char kInsertAugmentedTermQuery[] =
    // clang-format off
    "INSERT OR REPLACE INTO " TABLE_NAME "(field_name, "
    "term_id) VALUES (?, ?) RETURNING augmented_term_id";
// clang-format on

// The statement used to delete an augmented term ID from the database by
// augmented_term_id
static constexpr char kDeleteAugmentedTermQuery[] =
    // clang-format off
    "DELETE FROM " TABLE_NAME " WHERE augmented_term_id = ? "
    "RETURNING augmented_term_id";
// clang-format on

// The statement used fetch the augmented term ID by field name and term ID.
static constexpr char kGetAugmentedTermIdQuery[] =
    // clang-format off
    "SELECT augmented_term_id FROM " TABLE_NAME " "
    "WHERE field_name = ? AND term_id = ?";
// clang-format on

}  // namespace

AugmentedTermTable::AugmentedTermTable(sql::Database* db) : db_(db) {}
AugmentedTermTable::~AugmentedTermTable() = default;

bool AugmentedTermTable::Init() {
  if (!db_->is_open()) {
    LOG(WARNING) << "Faield to initialize augmented_term_table "
                 << "due to closed database";
    return false;
  }
  sql::Statement create_table(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateAugmentedTermTableQuery));
  DCHECK(create_table.is_valid()) << "Invalid create the table statement: \""
                                  << create_table.GetSQLStatement() << "\"";
  if (!create_table.Run()) {
    LOG(ERROR) << "Failed to create augmented_term_table";
    return false;
  }
  return true;
}

int64_t AugmentedTermTable::GetAugmentedTermId(const std::string& field_name,
                                               int64_t term_id) const {
  sql::Statement get_augmented_term_id(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetAugmentedTermIdQuery));
  DCHECK(get_augmented_term_id.is_valid())
      << "Invalid get augmented term ID statement: \""
      << get_augmented_term_id.GetSQLStatement() << "\"";
  get_augmented_term_id.BindString(0, field_name);
  get_augmented_term_id.BindInt64(1, term_id);
  if (get_augmented_term_id.Step()) {
    return get_augmented_term_id.ColumnInt64(0);
  }
  return -1;
}

int64_t AugmentedTermTable::GetOrCreateAugmentedTermId(
    const std::string& field_name,
    int64_t term_id) {
  int64_t augmented_term_id = GetAugmentedTermId(field_name, term_id);
  if (augmented_term_id != -1) {
    return augmented_term_id;
  }
  sql::Statement insert_augmented_term(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertAugmentedTermQuery));
  DCHECK(insert_augmented_term.is_valid())
      << "Invalid insert augmented term statement: \""
      << insert_augmented_term.GetSQLStatement() << "\"";
  insert_augmented_term.BindString(0, field_name);
  insert_augmented_term.BindInt64(1, term_id);
  if (insert_augmented_term.Step()) {
    return insert_augmented_term.ColumnInt64(0);
  }
  LOG(ERROR) << "Failed to insert augmented term " << field_name << ":"
             << term_id;
  return -1;
}

int64_t AugmentedTermTable::DeleteAugmentedTerm(int64_t augmented_term_id) {
  sql::Statement delete_augmented_term(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAugmentedTermQuery));
  DCHECK(delete_augmented_term.is_valid())
      << "Invalid delete augmented term statement: \""
      << delete_augmented_term.GetSQLStatement() << "\"";
  delete_augmented_term.BindInt64(0, augmented_term_id);
  if (!delete_augmented_term.Step()) {
    if (!delete_augmented_term.Succeeded()) {
      LOG(ERROR) << "Failed to delete augmented term " << augmented_term_id;
    }
    return -1;
  }
  return delete_augmented_term.ColumnInt64(0);
}

}  // namespace file_manager
