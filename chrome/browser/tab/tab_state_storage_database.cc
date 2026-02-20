// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_database.h"

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/pass_key.h"
#include "chrome/browser/tab/payload_util.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "chrome/browser/tab/protocol/token.pb.h"
#include "chrome/browser/tab/storage_id.h"
#include "chrome/browser/tab/storage_loaded_data.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp//absl/container/flat_hash_map.h"

namespace tabs {
namespace {

using OpenTransaction = TabStateStorageDatabase::OpenTransaction;

// Update log:
// ??-08-2025, Version 1: Initial version of the database schema.
// 11-11-2025, Version 2: Add window_tag and is_off_the_record columns.
// 19-11-2025, Version 3: Change storage id type from int to blob and use token.
// 27-03-2025, Version 4: Add divergent nodes table.
const int kCurrentVersionNumber = 4;

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
constexpr char kDivergentNodesTableName[] = "divergent_nodes";

static constexpr char kCreateTabSchemaSql[] =
    "CREATE TABLE IF NOT EXISTS nodes("
    "id BLOB PRIMARY KEY NOT NULL,"
    "window_tag TEXT NOT NULL,"
    "is_off_the_record INTEGER NOT NULL,"
    "type INTEGER NOT NULL,"
    "children BLOB,"
    "payload BLOB)";

static constexpr char kCreateDivergentNodesSchemaSql[] =
    "CREATE TABLE IF NOT EXISTS divergent_nodes("
    "id BLOB PRIMARY KEY NOT NULL,"
    "window_tag TEXT NOT NULL,"
    "is_off_the_record INTEGER NOT NULL,"
    "children BLOB)";

static constexpr char kCreateIndexSql[] =
    "CREATE INDEX IF NOT EXISTS nodes_window_index "
    "ON nodes(window_tag, is_off_the_record)";

static constexpr char kCreateDivergentNodesIndexSql[] =
    "CREATE INDEX IF NOT EXISTS divergent_nodes_window_index "
    "ON divergent_nodes(window_tag, is_off_the_record)";

bool ExecuteSql(sql::Database* db, base::cstring_view sql_command) {
  DCHECK(db->IsSQLValid(sql_command)) << sql_command << " is not valid SQL.";
  return db->Execute(sql_command);
}

bool CreateSchema(sql::Database* db, sql::MetaTable* meta_table) {
  DCHECK(db->HasActiveTransactions());

  if (!ExecuteSql(db, kCreateTabSchemaSql)) {
    DLOG(ERROR) << "Failed to create tab schema.";
    return false;
  }

  if (!ExecuteSql(db, kCreateDivergentNodesSchemaSql)) {
    DLOG(ERROR) << "Failed to create divergent nodes schema.";
    return false;
  }

  if (!ExecuteSql(db, kCreateIndexSql)) {
    DLOG(ERROR) << "Failed to create index.";
    return false;
  }

  if (!ExecuteSql(db, kCreateDivergentNodesIndexSql)) {
    DLOG(ERROR) << "Failed to create divergent nodes index.";
    return false;
  }
  return true;
}

// TODO(crbug.com/459435876): Add histograms and enums to track database
// initialization failures.
bool InitSchema(sql::Database* db, sql::MetaTable* meta_table) {
  bool has_metatable = meta_table->DoesTableExist(db);
  bool has_either_tables = db->DoesTableExist(kTabsTableName) ||
                           db->DoesTableExist(kDivergentNodesTableName);
  if (!has_metatable && has_either_tables) {
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

  // Version 3 -> Version 4: Add divergent nodes table.
  if (meta_table->GetVersionNumber() == 3) {
    if (!ExecuteSql(db, kCreateDivergentNodesSchemaSql) ||
        !ExecuteSql(db, kCreateDivergentNodesIndexSql)) {
      LOG(ERROR)
          << "TabStateStorageDatabase failed to upgrade from version 3 to "
             "version 4.";
      return false;
    }
  }

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
    const base::FilePath& profile_path,
    bool support_off_the_record_data)
    : profile_path_(profile_path),
      support_off_the_record_data_(support_off_the_record_data),
      db_(sql::DatabaseOptions().set_preload(true),
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
                                       std::string_view window_tag,
                                       bool is_off_the_record,
                                       TabStorageType type,
                                       std::vector<uint8_t> payload,
                                       std::vector<uint8_t> children) {
  if (!support_off_the_record_data_ && is_off_the_record) {
    DLOG(ERROR) << "OTR saves are not supported by this database.";
    // Pretend we succeeded to avoid rollback.
    return true;
  }
  DCHECK(OpenTransaction::IsValid(transaction));

  if (is_off_the_record) {
    auto maybe_payload = Seal(id, window_tag, payload);
    if (!maybe_payload) {
      return false;
    }
    payload = std::move(*maybe_payload);
  }

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
                                              std::string_view window_tag,
                                              bool is_off_the_record,
                                              std::vector<uint8_t> payload) {
  DCHECK(OpenTransaction::IsValid(transaction));

  if (is_off_the_record) {
    auto maybe_payload = Seal(id, window_tag, payload);
    if (!maybe_payload) {
      return false;
    }
    payload = std::move(*maybe_payload);
  }

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

bool TabStateStorageDatabase::SaveDivergentNode(OpenTransaction* transaction,
                                                StorageId id,
                                                std::string_view window_tag,
                                                bool is_off_the_record,
                                                std::vector<uint8_t> children) {
  DCHECK(OpenTransaction::IsValid(transaction));

  static constexpr char kInsertDivergentNodeSql[] =
      "INSERT OR REPLACE INTO divergent_nodes"
      "(id, window_tag, is_off_the_record, children)"
      "VALUES (?,?,?,?)";

  DCHECK(db_.IsSQLValid(kInsertDivergentNodeSql));

  sql::Statement write_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertDivergentNodeSql));

  write_statement.BindBlob(0, StorageIdToBlob(id));
  write_statement.BindString(1, window_tag);
  write_statement.BindInt(2, static_cast<int>(is_off_the_record));
  write_statement.BindBlob(3, std::move(children));

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
  if (!open_transaction_) {
    DCHECK_EQ(0, open_transaction_count_);
    open_transaction_.emplace(&db_, base::PassKey<TabStateStorageDatabase>());
    sql::Transaction* transaction_ptr =
        open_transaction_->GetTransaction(base::PassKey<TabStateStorageDatabase>());

    if (!transaction_ptr->Begin()) {
      DLOG(ERROR) << "Failed to begin transaction.";
      open_transaction_->MarkFailed();
    }
  }
  open_transaction_count_++;
  return &*open_transaction_;
}

bool TabStateStorageDatabase::CloseTransaction(
    OpenTransaction* open_transaction) {
  DCHECK(open_transaction_) << "There is no open transaction.";
  DCHECK_EQ(open_transaction, &*open_transaction_) << "Transaction mismatch.";
  DCHECK_GT(open_transaction_count_, 0);

  open_transaction_count_--;
  if (open_transaction_count_ > 0) {
    return true;
  }

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
    std::string_view window_tag,
    bool is_off_the_record,
    std::unique_ptr<StorageLoadedData::Builder> builder) {
  // UNION ALL is used since it is more performant than a JOIN and avoids
  // matching IDs between the two tables.
  static constexpr char kSelectAllNodesSql[] =
      "SELECT id, type, payload, children, 0 as is_divergent FROM nodes "
      "WHERE window_tag = ? AND is_off_the_record = ? "
      "UNION ALL "
      "SELECT id, 0 as type, NULL as payload, children, 1 as is_divergent FROM "
      "divergent_nodes "
      "WHERE window_tag = ? AND is_off_the_record = ?";
  sql::Statement select_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectAllNodesSql));
  select_statement.BindString(0, window_tag);
  select_statement.BindInt(1, static_cast<int>(is_off_the_record));
  select_statement.BindString(2, window_tag);
  select_statement.BindInt(3, static_cast<int>(is_off_the_record));
  while (select_statement.Step()) {
    StorageId id = StorageIdFromBlob(select_statement.ColumnBlob(0));
    TabStorageType type =
        static_cast<TabStorageType>(select_statement.ColumnInt(1));

    std::optional<base::span<const uint8_t>> children;
    if (type != TabStorageType::kTab) {
      children = select_statement.ColumnBlob(3);
    }
    const bool is_divergent = select_statement.ColumnBool(4);

    if (is_divergent) {
      builder->AddDivergentNode(id, type, children,
                                base::PassKey<TabStateStorageDatabase>());
      continue;
    }

    base::span<const uint8_t> payload = select_statement.ColumnBlob(2);

    if (is_off_the_record) {
      std::optional<std::vector<uint8_t>> open_payload =
          Open(id, window_tag, payload);
      if (!open_payload) {
        continue;
      }
      builder->AddNode(id, type, *open_payload, children,
                       base::PassKey<TabStateStorageDatabase>());
    } else {
      builder->AddNode(id, type, payload, children,
                       base::PassKey<TabStateStorageDatabase>());
    }
  }
  return builder->Build(base::PassKey<TabStateStorageDatabase>(), this);
}

int TabStateStorageDatabase::CountTabsForWindow(std::string_view window_tag,
                                                bool is_off_the_record) {
  static constexpr char kCountTabsForWindowSql[] =
      "SELECT COUNT(*) FROM nodes WHERE window_tag = ? AND is_off_the_record = "
      "? AND type = ?";
  sql::Statement count(
      db_.GetCachedStatement(SQL_FROM_HERE, kCountTabsForWindowSql));
  count.BindString(0, window_tag);
  count.BindInt(1, static_cast<int>(is_off_the_record));
  count.BindInt(2, static_cast<int>(TabStorageType::kTab));
  DCHECK(count.Step());
  return count.ColumnInt(0);
}

void TabStateStorageDatabase::ClearAllNodes() {
  static constexpr char kDeleteAllNodesSql[] = "DELETE FROM nodes";
  sql::Statement delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteAllNodesSql));
  delete_statement.Run();
}

void TabStateStorageDatabase::ClearAllDivergentNodes() {
  static constexpr char kDeleteAllDivergentNodesSql[] =
      "DELETE FROM divergent_nodes";
  sql::Statement delete_divergent_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteAllDivergentNodesSql));
  delete_divergent_statement.Run();
}

void TabStateStorageDatabase::ClearWindow(std::string_view window_tag) {
  static constexpr char kDeleteWindowSql[] =
      "DELETE FROM nodes WHERE window_tag = ?";
  sql::Statement delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteWindowSql));
  delete_statement.BindString(0, window_tag);
  delete_statement.Run();
}

void TabStateStorageDatabase::ClearDivergentNodesForWindow(
    std::string_view window_tag,
    bool is_off_the_record) {
  static constexpr char kDeleteDivergentNodesForWindowSql[] =
      "DELETE FROM divergent_nodes WHERE window_tag = ? AND "
      "is_off_the_record = ?";
  sql::Statement delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteDivergentNodesForWindowSql));
  delete_statement.BindString(0, window_tag);
  delete_statement.BindInt(1, static_cast<int>(is_off_the_record));
  delete_statement.Run();
}

void TabStateStorageDatabase::ClearDivergenceWindow(
    std::string_view window_tag) {
  static constexpr char kDeleteDivergenceWindowSql[] =
      "DELETE FROM divergent_nodes WHERE window_tag = ?";
  sql::Statement delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteDivergenceWindowSql));
  delete_statement.BindString(0, window_tag);
  delete_statement.Run();
}

bool TabStateStorageDatabase::ClearNodesForWindowExcept(
    std::string_view window_tag,
    bool is_off_the_record,
    const std::vector<StorageId>& ids) {
  const std::string id_placeholders =
      base::JoinString(std::vector<std::string_view>(ids.size(), "?"), ",");

  const std::string kDeleteNodesExceptSql =
      base::StrCat({"DELETE FROM nodes WHERE window_tag = ? AND "
                    "is_off_the_record = ? AND id NOT IN (",
                    id_placeholders, ")"});

  sql::Statement delete_statement(
      db_.GetUniqueStatement(kDeleteNodesExceptSql));
  delete_statement.BindString(0, window_tag);
  delete_statement.BindBool(1, is_off_the_record);

  for (size_t i = 0; i < ids.size(); i++) {
    delete_statement.BindBlob(i + 2, StorageIdToBlob(ids[i]));
  }
  return delete_statement.Run();
}

void TabStateStorageDatabase::SetKey(std::string window_tag,
                                     std::vector<uint8_t> key) {
  // TODO(crbug.com/462769977): Duplicate insertions seem to happen in tests
  // likely due to restarts that somehow don't trigger RemoveKey. This should be
  // investigated and fixed so this can be changed to a CHECK that insertion
  // is successful.
  keys_.insert_or_assign(std::move(window_tag), std::move(key));
}

void TabStateStorageDatabase::RemoveKey(std::string_view window_tag) {
  keys_.erase(window_tag);
}

std::optional<std::vector<uint8_t>> TabStateStorageDatabase::Seal(
    StorageId storage_id,
    std::string_view window_tag,
    base::span<const uint8_t> payload) {
  auto it = keys_.find(window_tag);
  if (it == keys_.end()) {
    LOG(WARNING) << "Failed to seal payload, no key found for window tag: "
                 << window_tag << " skipping save.";
    return std::nullopt;
  }
  return SealPayload(it->second, payload, storage_id);
}

std::optional<std::vector<uint8_t>> TabStateStorageDatabase::Open(
    StorageId storage_id,
    std::string_view window_tag,
    base::span<const uint8_t> payload) {
  auto it = keys_.find(window_tag);
  if (it == keys_.end()) {
    LOG(WARNING)
        << "Failed to open sealed payload, no key found for window tag: "
        << window_tag << " skipping restore.";
    return std::nullopt;
  }
  return OpenPayload(it->second, payload, storage_id);
}

#if defined(NDEBUG)
void TabStateStorageDatabase::PrintAll() {
  static constexpr char kSelectAllNodesSql[] =
      "SELECT id, window_tag, is_off_the_record, type, children FROM nodes";
  sql::Statement select_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectAllNodesSql));

  int start_int = 0;
  absl::flat_hash_map<StorageId, int> storage_id_to_int;
  std::stringstream ss;

  ss << "Nodes Table Dump:\n";
  while (select_statement.Step()) {
    StorageId id = StorageIdFromBlob(select_statement.ColumnBlob(0));
    if (!storage_id_to_int.contains(id)) {
      start_int++;
      storage_id_to_int[id] = start_int;
    }

    std::string window_tag = select_statement.ColumnString(1);
    bool is_off_the_record = select_statement.ColumnInt(2);
    TabStorageType type =
        static_cast<TabStorageType>(select_statement.ColumnInt(3));

    base::span<const uint8_t> children_unparsed =
        select_statement.ColumnBlob(4);
    tabs_pb::Children children;
    children.ParseFromArray(children_unparsed.data(), children_unparsed.size());
    std::string children_str;

    if (children.storage_id_size() != 0) {
      for (int i = 0; i < children.storage_id_size(); i++) {
        tabs_pb::Token child = children.storage_id().at(i);
        StorageId child_id = StorageIdFromTokenProto(child);

        if (!storage_id_to_int.contains(child_id)) {
          start_int++;
          storage_id_to_int[child_id] = start_int;
        }

        children_str += base::NumberToString(storage_id_to_int[child_id]);
        if (i + 1 != children.storage_id_size()) {
          children_str += ", ";
        }
      }
      children_str = ", children=" + children_str;
    }

    ss << "Node: id=" << storage_id_to_int[id] << ", window_tag=" << window_tag
       << ", is_off_the_record=" << is_off_the_record
       << ", type=" << static_cast<int>(type) << children_str << "\n";
  }

  ss << "\nInt to Storage Id Map:\n";
  for (const auto& [storage_id, temp_int] : storage_id_to_int) {
    ss << "Entry: storage_id=" << storage_id.ToString()
       << ", temp_int=" << temp_int << "\n";
  }
  VLOG(1) << ss.str();
}
#endif

}  // namespace tabs
