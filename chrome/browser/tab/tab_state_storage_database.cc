// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_database.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/types/pass_key.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace tabs {
namespace {

using OpenTransaction = TabStateStorageDatabase::OpenTransaction;

// Update log:
// ??-08-2025, Version 1: Initial version of the database schema.
// 11-11-2025, Version 2: Add window_tag and is_off_the_record columns.
// 19-11-2025, Version 3: Change storage id type from int to blob and use token.
const int kCurrentVersionNumber = 3;

// The last version of the database schema that is compatible with the current
// version. Any changes made to the database schema that would break
// compatibility with the current version should increment this number to
// trigger a raze and rebuild of the database schema. NOTE: once this database
// become the primary database for restore this number should NEVER be
// incremented and we should use a graceful upgrade path. We can however,
// consider inlining the upgrade path and incrementing this number if a
// significant enough time (O(years)) have passed that there is no longer a
// reasonable expectation that out-of-date users would be able to restore their
// session.
const int kCompatibleVersionNumber = 3;

static_assert(
    kCurrentVersionNumber >= kCompatibleVersionNumber,
    "Current version must be greater than or equal to compatible version.");

constexpr char kTabsTableName[] = "nodes";

bool ExecuteSql(sql::Database* db, base::cstring_view sql_command) {
  DCHECK(db->IsSQLValid(sql_command)) << sql_command << " is not valid SQL.";
  return db->Execute(sql_command);
}

bool CreateSchema(sql::Database* db, sql::MetaTable* meta_table) {
  DCHECK(db->HasActiveTransactions());

  static constexpr char kCreateTabSchemaSql[] =
      "CREATE TABLE IF NOT EXISTS nodes("
      "id BLOB PRIMARY KEY NOT NULL,"
      "window_tag TEXT NOT NULL,"
      "is_off_the_record INTEGER NOT NULL,"
      "type INTEGER NOT NULL,"
      "children BLOB,"
      "payload BLOB)";

  static constexpr char kCreateIndexSql[] =
      "CREATE INDEX IF NOT EXISTS nodes_window_index "
      "ON nodes(window_tag, is_off_the_record)";

  if (!ExecuteSql(db, kCreateTabSchemaSql)) {
    DLOG(ERROR) << "Failed to create tab schema.";
    return false;
  }

  if (!ExecuteSql(db, kCreateIndexSql)) {
    DLOG(ERROR) << "Failed to create index.";
    return false;
  }
  return true;
}

// TODO(crbug.com/459435876): Add histograms and enums to track database
// initialization failures.
bool InitSchema(sql::Database* db, sql::MetaTable* meta_table) {
  bool has_metatable = meta_table->DoesTableExist(db);

  if (!has_metatable && db->DoesTableExist(kTabsTableName)) {
    db->Raze();
  }

  if (sql::MetaTable::RazeIfIncompatible(db, kCompatibleVersionNumber,
                                         kCurrentVersionNumber) ==
      sql::RazeIfIncompatibleResult::kFailed) {
    LOG(ERROR) << "TabStateStorageDatabase failed to raze when an incompatible "
                  "version was detected.";
    return false;
  }

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    DLOG(ERROR) << "Transaction could not be started.";
    return false;
  }

  if (!meta_table->Init(db, kCurrentVersionNumber, kCompatibleVersionNumber)) {
    LOG(ERROR) << "TabStateStorageDatabase failed to initialize meta table.";
    return false;
  }

  // This implies that the database was rolled back, without a downgrade path.
  // This should never happen.
  if (meta_table->GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(ERROR)
        << "TabStateStorageDatabase has a compatible version greater than the "
           "current version. Did a rollback occur without a downgrade path?";
    return false;
  }

  // Do not cache if the schema exists, it may have been razed earlier and needs
  // to be recreated.
  if (!db->DoesTableExist(kTabsTableName) && !CreateSchema(db, meta_table)) {
    LOG(ERROR) << "TabStateStorageDatabase failed to create schema.";
    return false;
  }

  // Any graceful upgrade logic when changing versions should go here in version
  // upgrade order.

  return meta_table->SetVersionNumber(kCurrentVersionNumber) &&
         meta_table->SetCompatibleVersionNumber(kCompatibleVersionNumber) &&
         transaction.Commit();
}

}  // namespace

OpenTransaction::OpenTransaction(sql::Database* db,
                                 base::PassKey<TabStateStorageDatabase>)
    : transaction_(db) {}

OpenTransaction::~OpenTransaction() = default;

bool OpenTransaction::HasFailed() {
  return mark_failed_;
}

void OpenTransaction::MarkFailed() {
  mark_failed_ = true;
}

sql::Transaction* OpenTransaction::GetTransaction(
    base::PassKey<TabStateStorageDatabase>) {
  return &transaction_;
}

// static
bool TabStateStorageDatabase::OpenTransaction::IsValid(
    OpenTransaction* transaction) {
  return transaction && !transaction->HasFailed();
}

TabStateStorageDatabase::TabStateStorageDatabase(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),

      db_(sql::DatabaseOptions().set_preload(true).set_exclusive_locking(true),
          sql::Database::Tag("TabStateStorage")) {}

TabStateStorageDatabase::~TabStateStorageDatabase() = default;

bool TabStateStorageDatabase::Initialize() {
  base::FilePath db_dir = profile_path_.Append(FILE_PATH_LITERAL("Tabs"));
  if (!base::CreateDirectory(db_dir)) {
    LOG(ERROR) << "Failed to create directory for tab state storage database: "
               << db_dir;
    return false;
  }

  const base::FilePath db_path = db_dir.Append(FILE_PATH_LITERAL("TabDB"));
  if (!db_.Open(db_path)) {
    LOG(ERROR) << "Failed to open tab state storage database: "
               << db_.GetErrorMessage();
    return false;
  }

  if (!InitSchema(&db_, &meta_table_)) {
    DLOG(ERROR) << "Failed to create schema for tab state storage database: "
                << db_.GetErrorMessage();
    db_.Close();
    return false;
  }

  return true;
}

bool TabStateStorageDatabase::SaveNode(OpenTransaction* transaction,
                                       StorageId id,
                                       std::string window_tag,
                                       bool is_off_the_record,
                                       TabStorageType type,
                                       std::vector<uint8_t> payload,
                                       std::vector<uint8_t> children) {
  DCHECK(OpenTransaction::IsValid(transaction));

  static constexpr char kInsertNodeSql[] =
      "INSERT OR REPLACE INTO nodes"
      "(id, window_tag, is_off_the_record, type, payload, children)"
      "VALUES (?,?,?,?,?,?)";

  DCHECK(db_.IsSQLValid(kInsertNodeSql));

  sql::Statement write_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertNodeSql));

  write_statement.BindBlob(0, StorageIdToBlob(id));
  write_statement.BindString(1, window_tag);
  write_statement.BindInt(2, static_cast<int>(is_off_the_record));
  write_statement.BindInt(3, static_cast<int>(type));
  write_statement.BindBlob(4, std::move(payload));
  write_statement.BindBlob(5, std::move(children));

  return write_statement.Run();
}

bool TabStateStorageDatabase::SaveNodePayload(OpenTransaction* transaction,
                                              StorageId id,
                                              std::vector<uint8_t> payload) {
  DCHECK(OpenTransaction::IsValid(transaction));

  static constexpr char kUpdatePayloadSql[] =
      "UPDATE nodes "
      "SET payload = ? "
      "WHERE id = ?";

  DCHECK(db_.IsSQLValid(kUpdatePayloadSql));

  sql::Statement write_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kUpdatePayloadSql));

  write_statement.BindBlob(0, std::move(payload));
  write_statement.BindBlob(1, StorageIdToBlob(id));

  return write_statement.Run();
}

bool TabStateStorageDatabase::SaveNodeChildren(OpenTransaction* transaction,
                                               StorageId id,
                                               std::vector<uint8_t> children) {
  DCHECK(OpenTransaction::IsValid(transaction));

  static constexpr char kUpdateChildrenSql[] =
      "UPDATE nodes "
      "SET children = ? "
      "WHERE id = ?";

  DCHECK(db_.IsSQLValid(kUpdateChildrenSql));

  sql::Statement write_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kUpdateChildrenSql));

  write_statement.BindBlob(0, std::move(children));
  write_statement.BindBlob(1, StorageIdToBlob(id));

  return write_statement.Run();
}

bool TabStateStorageDatabase::RemoveNode(OpenTransaction* transaction,
                                         StorageId id) {
  DCHECK(OpenTransaction::IsValid(transaction));

  static constexpr char kDeleteNodeSql[] =
      "DELETE FROM nodes "
      "WHERE id = ?";

  DCHECK(db_.IsSQLValid(kDeleteNodeSql));

  sql::Statement write_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteNodeSql));

  write_statement.BindBlob(0, StorageIdToBlob(id));
  return write_statement.Run();
}

OpenTransaction* TabStateStorageDatabase::CreateTransaction() {
  DCHECK(!open_transaction_) << "An open transaction already exists.";

  open_transaction_.emplace(&db_, base::PassKey<TabStateStorageDatabase>());
  sql::Transaction* transaction_ptr = open_transaction_->GetTransaction(
      base::PassKey<TabStateStorageDatabase>());

  if (!transaction_ptr->Begin()) {
    DLOG(ERROR) << "Failed to begin transaction.";
    open_transaction_->MarkFailed();
  }

  return &*open_transaction_;
}

bool TabStateStorageDatabase::CloseTransaction(
    OpenTransaction* open_transaction) {
  DCHECK(open_transaction_) << "There is no open transaction.";
  DCHECK_EQ(open_transaction, &*open_transaction_) << "Transaction mismatch.";
  sql::Transaction* transaction = open_transaction->GetTransaction(
      base::PassKey<TabStateStorageDatabase>());

  bool success = false;
  if (open_transaction->HasFailed()) {
    transaction->Rollback();
    DLOG(ERROR) << "Transaction rolled back.";
  } else {
    success = transaction->Commit();
    if (!success) {
      DLOG(ERROR) << "Failed to commit transaction.";
      // TODO(crbug.com/454005648): If possible, record the reason for commit
      // failure here.
    }
  }

  open_transaction_.reset();
  return success;
}

std::unique_ptr<StorageLoadedData> TabStateStorageDatabase::LoadAllNodes(
    const std::string& window_tag,
    bool is_off_the_record,
    std::unique_ptr<StorageLoadedData::Builder> builder) {
  static constexpr char kSelectAllNodesSql[] =
      "SELECT id, type, payload, children FROM nodes "
      "WHERE window_tag = ? AND is_off_the_record = ?";
  sql::Statement select_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectAllNodesSql));
  select_statement.BindString(0, window_tag);
  select_statement.BindInt(1, static_cast<int>(is_off_the_record));
  while (select_statement.Step()) {
    StorageId id = StorageIdFromBlob(select_statement.ColumnBlob(0));
    TabStorageType type =
        static_cast<TabStorageType>(select_statement.ColumnInt(1));
    builder->AddNode(id, type, select_statement.ColumnBlob(2),
                     base::PassKey<TabStateStorageDatabase>());
    builder->AddChildren(id, type, select_statement.ColumnBlob(3),
                         base::PassKey<TabStateStorageDatabase>());
  }
  return builder->Build();
}

void TabStateStorageDatabase::ClearAllNodes() {
  static constexpr char kDeleteAllNodesSql[] = "DELETE FROM nodes";
  sql::Statement delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteAllNodesSql));
  delete_statement.Run();
}

void TabStateStorageDatabase::ClearWindow(const std::string& window_tag) {
  static constexpr char kDeleteWindowSql[] =
      "DELETE FROM nodes WHERE window_tag = ?";
  sql::Statement delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteWindowSql));
  delete_statement.BindString(0, window_tag);
  delete_statement.Run();
}

}  // namespace tabs
