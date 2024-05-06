// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/posting_list_table.h"

#include "sql/statement.h"

namespace file_manager {

namespace {

#define POSTING_LIST_TABLE "posting_list_table"
#define AUGMENTED_TERM_ID "augmented_term_id"
#define URL_ID "url_id"
#define POSTING_LIST_INDEX "posting_list_index"
#define URL_ID_INDEX "url_id_index"

// The statement used to create the posting list table.
static constexpr char kCreatePostingListTableQuery[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS " POSTING_LIST_TABLE "("
      AUGMENTED_TERM_ID " INTEGER NOT NULL,"
      URL_ID " INTEGER NOT NULL,"
      "FOREIGN KEY(" AUGMENTED_TERM_ID ") REFERENCES augmented_term_table("
          AUGMENTED_TERM_ID "),"
      "FOREIGN KEY(" URL_ID ") REFERENCES file_info_table(url_id),"
      "PRIMARY KEY (" AUGMENTED_TERM_ID ", " URL_ID "))";
// clang-format on

// The statement that creates an inverted index from augmented_term_id to
// url_id. This facilitates quick retrieval of all URL IDs for the given
// augmented term.
static constexpr char kCreatePostingIndexQuery[] =
    // clang-format off
    "CREATE INDEX " POSTING_LIST_INDEX " ON " POSTING_LIST_TABLE "("
    AUGMENTED_TERM_ID ")";
// clang-format on

// The statement that creates an plain index from URL IDs to augmented term IDs.
// This facilitates quick retrieval of all augmented terms associated with the
// given URL ID (and thus, a file).
static constexpr char kCreateUrlIndexQuery[] =
    // clang-format off
    "CREATE INDEX " URL_ID_INDEX " ON posting_list_table(" URL_ID ")";
// clang-format on

// The statement used to insert a new association between the augmented term ID
// and the URL ID.
static constexpr char kInsertAssociationQuery[] =
    // clang-format off
    "INSERT OR IGNORE INTO " POSTING_LIST_TABLE "(" AUGMENTED_TERM_ID ", "
    URL_ID ") VALUES (?, ?)";
// clang-format on

// The statement used to delete an association between the augmented term ID
// and the URL ID.
static constexpr char kDeleteAssociationQuery[] =
    // clang-format off
    "DELETE FROM " POSTING_LIST_TABLE " WHERE " AUGMENTED_TERM_ID "=? "
    "AND " URL_ID "=?";
// clang-format on

// A query that fetches all URL IDs for the given augmented term ID. This
// query utilizes the posting_list_index.
static constexpr char kGetUrlIdsForTermQuery[] =
    // clang-format off
    "SELECT " URL_ID " FROM " POSTING_LIST_TABLE " INDEXED BY "
    POSTING_LIST_INDEX " WHERE " AUGMENTED_TERM_ID "=?";
// clang-format on

// A query that fetches all augmented term IDs for the given augmented URL ID.
// This query utilizes the url_id_index.
static constexpr char kGetAugmentedTermIdsForUrlQuery[] =
    // clang-format off
    "SELECT " AUGMENTED_TERM_ID " FROM " POSTING_LIST_TABLE " INDEXED BY "
    URL_ID_INDEX " WHERE " URL_ID "=?";
// clang-format on
}  // namespace

PostingListTable::PostingListTable(sql::Database* db) : db_(db) {}
PostingListTable::~PostingListTable() = default;

bool PostingListTable::Init() {
  if (!db_->is_open()) {
    LOG(WARNING) << "Faield to initialize " << POSTING_LIST_TABLE
                 << "due to closed database";
    return false;
  }
  sql::Statement create_table(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreatePostingListTableQuery));
  DCHECK(create_table.is_valid()) << "Invalid create table statement: \""
                                  << create_table.GetSQLStatement() << "\"";
  if (!create_table.Run()) {
    LOG(ERROR) << "Failed to create table " << POSTING_LIST_TABLE;
    return false;
  }
  sql::Statement create_posting_index(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreatePostingIndexQuery));
  DCHECK(create_posting_index.is_valid())
      << "Invalid create index statement: \""
      << create_posting_index.GetSQLStatement() << "\"";
  if (!create_posting_index.Run()) {
    LOG(ERROR) << "Failed to create posting index";
    return false;
  }
  sql::Statement create_url_index(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateUrlIndexQuery));
  DCHECK(create_url_index.is_valid())
      << "Invalid create index statement: \""
      << create_url_index.GetSQLStatement() << "\"";
  if (!create_url_index.Run()) {
    LOG(ERROR) << "Failed to create url index";
    return false;
  }
  return true;
}

int32_t PostingListTable::AddToPostingList(int64_t augmented_term_id,
                                           int64_t url_id) {
  sql::Statement add_association(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertAssociationQuery));
  DCHECK(add_association.is_valid())
      << "Invalid insert statement: \"" << add_association.GetSQLStatement()
      << "\"";
  add_association.BindInt64(0, augmented_term_id);
  add_association.BindInt64(1, url_id);
  if (!add_association.Run()) {
    LOG(ERROR) << "Failed to create association between augmented term and URL";
    return 0;
  }
  return db_->GetLastChangeCount();
}

int32_t PostingListTable::DeleteFromPostingList(int64_t augmented_term_id,
                                                int64_t url_id) {
  sql::Statement delete_association(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAssociationQuery));
  DCHECK(delete_association.is_valid())
      << "Invalid delete statement: \"" << delete_association.GetSQLStatement()
      << "\"";
  delete_association.BindInt64(0, augmented_term_id);
  delete_association.BindInt64(1, url_id);
  if (!delete_association.Run()) {
    LOG(ERROR) << "Failed to delete association between augmented term and URL";
    return 0;
  }
  return db_->GetLastChangeCount();
}

std::set<int64_t> PostingListTable::GetUrlIdsForTerm(
    int64_t augmented_term_id) const {
  sql::Statement get_url_ids(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetUrlIdsForTermQuery));
  DCHECK(get_url_ids.is_valid()) << "Invalid select statement: \""
                                 << get_url_ids.GetSQLStatement() << "\"";
  get_url_ids.BindInt64(0, augmented_term_id);
  std::set<int64_t> url_ids;
  while (get_url_ids.Step()) {
    url_ids.emplace(get_url_ids.ColumnInt64(0));
  }
  return url_ids;
}

const std::set<int64_t> PostingListTable::GetAugmentedTermIdsForUrl(
    int64_t url_id) const {
  sql::Statement get_augmented_term_ids(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetAugmentedTermIdsForUrlQuery));
  DCHECK(get_augmented_term_ids.is_valid())
      << "Invalid select statement: \""
      << get_augmented_term_ids.GetSQLStatement() << "\"";
  get_augmented_term_ids.BindInt64(0, url_id);
  std::set<int64_t> augmented_term_ids;
  while (get_augmented_term_ids.Step()) {
    augmented_term_ids.emplace(get_augmented_term_ids.ColumnInt64(0));
  }
  return augmented_term_ids;
}

}  // namespace file_manager
