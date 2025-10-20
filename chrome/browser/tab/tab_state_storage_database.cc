// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_database.h"

#include <memory>

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

constexpr char kTabsTableName[] = "nodes";

bool CreateTable(sql::Database* db, base::cstring_view table_creation_script) {
  DCHECK(db->IsSQLValid(table_creation_script));
  return db->Execute(table_creation_script);
}

bool CreateSchema(sql::Database* db, sql::MetaTable* meta_table) {
  DCHECK(db->HasActiveTransactions());

  static constexpr char kCreateTabSchemaSql[] =
      "CREATE TABLE IF NOT EXISTS nodes("
      "id INTEGER PRIMARY KEY NOT NULL,"
      "type INTEGER NOT NULL,"
      "children BLOB,"
      "payload BLOB)";

  return CreateTable(db, kCreateTabSchemaSql);
}

bool InitSchema(sql::Database* db, sql::MetaTable* meta_table) {
  bool has_metatable = meta_table->DoesTableExist(db);
  bool has_schema = db->DoesTableExist(kTabsTableName);

  if (!has_metatable && has_schema) {
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

  if (!has_schema && !CreateSchema(db, meta_table)) {
    return false;
  }

  return meta_table->SetVersionNumber(kCurrentVersionNumber) &&
         meta_table->SetCompatibleVersionNumber(kCompatibleVersionNumber) &&
         transaction.Commit();
}

}  // namespace

TabStateStorageDatabase::Transaction::Transaction(
    std::unique_ptr<sql::Transaction> transaction)
    : transaction_(std::move(transaction)) {}

TabStateStorageDatabase::Transaction::~Transaction() {
  if (transaction_) {
    transaction_->Rollback();
  }
}

bool TabStateStorageDatabase::Transaction::Begin() {
  DCHECK(transaction_) << "Transaction already closed.";
  return transaction_->Begin();
}

void TabStateStorageDatabase::Transaction::Rollback() {
  DCHECK(transaction_) << "Transaction already closed.";
  transaction_.release()->Rollback();
}

bool TabStateStorageDatabase::Transaction::IsOpen() {
  return transaction_.get() != nullptr;
}

bool TabStateStorageDatabase::Transaction::Commit() {
  DCHECK(transaction_) << "Transaction already closed.";
  return transaction_.release()->Commit();
}

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

bool TabStateStorageDatabase::SaveNode(Transaction* transaction,
                                       int id,
                                       TabStorageType type,
                                       std::string payload,
                                       std::string children) {
  CHECK(db_);
  DCHECK(transaction && transaction->IsOpen());

  static constexpr char kInsertTabSql[] =
      "INSERT OR REPLACE INTO nodes"
      "(id, type, payload, children)"
      "VALUES (?,?,?,?)";

  DCHECK(db_->IsSQLValid(kInsertTabSql));

  sql::Statement write_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertTabSql));

  write_statement.BindInt(0, id);
  write_statement.BindInt(1, static_cast<int>(type));
  write_statement.BindBlob(2, std::move(payload));
  write_statement.BindBlob(3, std::move(children));

  return write_statement.Run();
}

bool TabStateStorageDatabase::SaveNodeChildren(Transaction* transaction,
                                               int id,
                                               std::string children) {
  CHECK(db_);
  DCHECK(transaction && transaction->IsOpen());

  static constexpr char kUpdateChildrenSql[] =
      "UPDATE nodes"
      "SET children = ?"
      "WHERE id = ?";

  DCHECK(db_->IsSQLValid(kUpdateChildrenSql));

  sql::Statement write_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kUpdateChildrenSql));

  write_statement.BindInt(0, id);
  write_statement.BindBlob(1, std::move(children));

  return write_statement.Run();
}

bool TabStateStorageDatabase::RemoveNode(Transaction* transaction, int id) {
  CHECK(db_);
  DCHECK(transaction && transaction->IsOpen());

  static constexpr char kDeleteChildrenSql[] =
      "DELETE FROM nodes"
      "WHERE id = ?";

  DCHECK(db_->IsSQLValid(kDeleteChildrenSql));

  sql::Statement write_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteChildrenSql));

  write_statement.BindInt(0, id);
  return write_statement.Run();
}

std::unique_ptr<TabStateStorageDatabase::Transaction>
TabStateStorageDatabase::CreateTransaction() {
  std::unique_ptr<sql::Transaction> sql_transaction =
      std::make_unique<sql::Transaction>(db_.get());
  return std::make_unique<Transaction>(std::move(sql_transaction));
}

std::vector<NodeState> TabStateStorageDatabase::LoadAllNodes() {
  std::vector<NodeState> entries;
  static constexpr char kSelectAllTabsSql[] =
      "SELECT id, type, payload, children FROM nodes";
  sql::Statement select_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectAllTabsSql));
  while (select_statement.Step()) {
    NodeState entry;
    entry.id = select_statement.ColumnInt(0);
    entry.type = static_cast<TabStorageType>(select_statement.ColumnInt(1));
    entry.payload = select_statement.ColumnBlobAsString(2);
    entry.children = select_statement.ColumnBlobAsString(3);
    entries.emplace_back(std::move(entry));
  }
  return entries;
}

}  // namespace tabs
