// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_database.h"

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace tabs {
namespace {

const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;

constexpr char kTabsTableName[] = "tab_state";
constexpr char kTabCollectionsTableName[] = "tab_collections";

bool CreateTable(sql::Database* db, base::cstring_view table_creation_script) {
  DCHECK(db->IsSQLValid(table_creation_script));
  return db->Execute(table_creation_script);
}

bool CreateSchema(sql::Database* db, sql::MetaTable* meta_table) {
  DCHECK(db->HasActiveTransactions());

  static constexpr char kCreateTabSchemaSql[] =
      "CREATE TABLE IF NOT EXISTS tab_state("
      "id INTEGER PRIMARY KEY NOT NULL,"
      "parent INTEGER NOT NULL,"
      "position TEXT NOT NULL,"
      "type INTEGER NOT NULL,"
      "payload TEXT NOT NULL)"
      "WITHOUT ROWID";

  if (!CreateTable(db, kCreateTabSchemaSql)) {
    return false;
  }

  static constexpr char kCreateTabCollectionSchemaSql[] =
      "CREATE TABLE IF NOT EXISTS tab_collections("
      "parent_id INTEGER NOT NULL,"
      "id INTEGER NOT NULL,"
      "position TEXT NOT NULL,"
      "PRIMARY KEY (parent_id, position))";
  return CreateTable(db, kCreateTabCollectionSchemaSql);
}

bool InitSchema(sql::Database* db, sql::MetaTable* meta_table) {
  bool has_metatable = meta_table->DoesTableExist(db);
  bool has_schema = db->DoesTableExist(kTabsTableName);
  bool has_collections_schema = db->DoesTableExist(kTabCollectionsTableName);

  if (!has_metatable && (has_schema || has_collections_schema)) {
    db->Raze();
  }

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    DLOG(ERROR) << "Transaction could not be started.";
    return false;
  }

  if (!meta_table->Init(db, kCurrentVersionNumber, kCompatibleVersionNumber)) {
    return false;
  }

  if (meta_table->GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    return false;
  }

  if ((!has_schema || !has_collections_schema) &&
      !CreateSchema(db, meta_table)) {
    return false;
  }

  return meta_table->SetVersionNumber(kCurrentVersionNumber) &&
         meta_table->SetCompatibleVersionNumber(kCompatibleVersionNumber) &&
         transaction.Commit();
}

}  // namespace

TabStateStorageDatabase::TabStateStorageDatabase(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      db_(std::make_unique<sql::Database>(
          sql::Database::Tag("TabStateStorage"))),
      meta_table_(std::make_unique<sql::MetaTable>()) {}

TabStateStorageDatabase::~TabStateStorageDatabase() = default;

bool TabStateStorageDatabase::Initialize() {
  CHECK(db_);
  CHECK(meta_table_);

  base::FilePath db_dir = profile_path_.Append(FILE_PATH_LITERAL("Tabs"));
  if (!base::CreateDirectory(db_dir)) {
    LOG(ERROR) << "Failed to create directory for tab state storage database: "
               << db_dir;
    return false;
  }

  const base::FilePath db_path = db_dir.Append(FILE_PATH_LITERAL("TabDB"));
  if (!db_->Open(db_path)) {
    LOG(ERROR) << "Failed to open tab state storage database: "
               << db_->GetErrorMessage();
    return false;
  }

  if (!InitSchema(db_.get(), meta_table_.get())) {
    DLOG(ERROR) << "Failed to create schema for tab state storage database: "
                << db_->GetErrorMessage();
    db_->Close();
    return false;
  }

  return true;
}

bool TabStateStorageDatabase::SaveTabState(int id,
                                           int parent,
                                           std::string position,
                                           tabs_pb::TabState tab_state) {
  CHECK(db_);

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kInsertTabSql[] =
      "INSERT OR REPLACE INTO tab_state"
      "(id, parent, position, type, payload)"
      "VALUES (?,?,?,?,?)";

  DCHECK(db_->IsSQLValid(kInsertTabSql));

  sql::Statement write_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertTabSql));

  write_statement.BindInt(0, id);
  write_statement.BindInt(1, parent);
  write_statement.BindString(2, position);
  write_statement.BindInt(3, 1);
  std::string data;
  tab_state.SerializeToString(&data);
  write_statement.BindString(4, data);

  if (!write_statement.Run()) {
    DLOG(ERROR) << "Could not write to tabs table.";
    return false;
  }

  static constexpr char kInsertTabCollectionSql[] =
      "INSERT OR REPLACE INTO tab_collections"
      "(parent_id, id, position)"
      "VALUES (?,?,?)";

  DCHECK(db_->IsSQLValid(kInsertTabCollectionSql));

  sql::Statement collection_write_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertTabCollectionSql));

  collection_write_statement.BindInt(0, parent);
  collection_write_statement.BindInt(1, id);
  collection_write_statement.BindString(2, position);

  if (!collection_write_statement.Run()) {
    DLOG(ERROR) << "Could not write to tab_collection table.";
    return false;
  }

  return transaction.Commit();
}

std::vector<tabs_pb::TabState> TabStateStorageDatabase::LoadAllTabStates() {
  std::vector<tabs_pb::TabState> tab_states;
  static constexpr char kSelectAllTabsSql[] = "SELECT payload FROM tab_state";
  sql::Statement select_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectAllTabsSql));
  while (select_statement.Step()) {
    tabs_pb::TabState tab_state;
    if (tab_state.ParseFromString(select_statement.ColumnString(0))) {
      tab_states.emplace_back(std::move(tab_state));
    }
  }
  return tab_states;
}

}  // namespace tabs
