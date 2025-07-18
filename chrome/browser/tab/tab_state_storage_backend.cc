// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_state_storage_backend.h"

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "sql/transaction.h"

namespace tabs {
namespace {

const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;

constexpr char kTabsTableName[] = "tab_state";

constexpr base::TaskTraits kDBTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

bool CreateSchema(sql::Database* db, sql::MetaTable* meta_table) {
  DCHECK(db->HasActiveTransactions());

  static constexpr char kCreateTabSchemaSql[] =
      "CREATE TABLE IF NOT EXISTS tabs("
      "id INTEGER PRIMARY KEY NOT NULL,"
      "parent INTEGER NOT NULL,"
      "position TEXT NOT NULL,"
      "type INTEGER NOT NULL,"
      "payload TEXT NOT NULL)"
      "WITHOUT ROWID";

  DCHECK(db->IsSQLValid(kCreateTabSchemaSql));
  return db->Execute(kCreateTabSchemaSql);
}

bool InitSchema(sql::Database* db, sql::MetaTable* meta_table) {
  bool has_metatable = meta_table->DoesTableExist(db);
  bool has_schema = db->DoesTableExist(kTabsTableName);

  if (!has_metatable && has_schema) {
    db->Raze();
  }

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
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

bool InitOnDBSequence(base::FilePath profile_dir,
                      sql::Database* db,
                      sql::MetaTable* meta_table) {
  CHECK(db);
  CHECK(meta_table);

  base::FilePath db_dir = profile_dir.Append(FILE_PATH_LITERAL("Tabs"));
  if (!base::CreateDirectory(db_dir)) {
    LOG(ERROR) << "Failed to create directory for tab state storage database: "
               << db_dir;
    return false;
  }

  const base::FilePath db_path = db_dir.Append(FILE_PATH_LITERAL("TabDB"));
  if (!db->Open(db_path)) {
    LOG(ERROR) << "Failed to open tab state storage database: "
               << db->GetErrorMessage();
    return false;
  }

  if (!InitSchema(db, meta_table)) {
    DLOG(ERROR) << "Failed to create schema for tab state storage database: "
                << db->GetErrorMessage();
    db->Close();
    return false;
  }

  return true;
}

bool WriteTab(sql::Database* db,
              int id,
              int parent,
              std::string position,
              tabs_pb::TabState tab_state) {
  CHECK(db);
  static constexpr char kInsertTabSql[] =
      "INSERT OR REPLACE INTO tabs"
      "(id, parent, position, type, payload)"
      "VALUES (?,?,?,?,?)";

  DCHECK(db->IsSQLValid(kInsertTabSql));

  sql::Statement write_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kInsertTabSql));

  write_statement.BindInt(0, id);
  write_statement.BindInt(1, parent);
  write_statement.BindString(2, position);
  write_statement.BindInt(3, 1);
  std::string data;
  tab_state.SerializeToString(&data);
  write_statement.BindString(4, data);

  return write_statement.Run();
}

}  // namespace

TabStateStorageBackend::TabStateStorageBackend(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      db_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kDBTaskTraits)),
      db_(std::make_unique<sql::Database>(
          sql::Database::Tag("TabStateStorage"))),
      meta_table_(std::make_unique<sql::MetaTable>()) {}

TabStateStorageBackend::~TabStateStorageBackend() = default;

void TabStateStorageBackend::Initialize() {
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&InitOnDBSequence, profile_path_,
                     base::Unretained(db_.get()),
                     base::Unretained(meta_table_.get())),
      base::BindOnce(&TabStateStorageBackend::OnDBReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::SaveTabState(int id,
                                          int parent,
                                          std::string position,
                                          tabs_pb::TabState tab_state) {
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WriteTab, base::Unretained(db_.get()), id, parent,
                     position, tab_state),
      base::BindOnce(&TabStateStorageBackend::OnWrite,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabStateStorageBackend::LoadAllTabStates(
    base::OnceCallback<void(std::vector<tabs_pb::TabState>)> callback) {
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TabStateStorageBackend::ReadAllTabs,
                     base::Unretained(this)),
      base::BindOnce(&TabStateStorageBackend::OnAllTabsRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TabStateStorageBackend::OnDBReady(bool success) {}

void TabStateStorageBackend::OnWrite(bool success) {}

std::vector<tabs_pb::TabState> TabStateStorageBackend::ReadAllTabs() {
  std::vector<tabs_pb::TabState> tab_states;
  static constexpr char kSelectAllTabsSql[] = "SELECT payload FROM tabs";
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

void TabStateStorageBackend::OnAllTabsRead(
    base::OnceCallback<void(std::vector<tabs_pb::TabState>)> callback,
    std::vector<tabs_pb::TabState> result) {
  std::move(callback).Run(std::move(result));
}

}  // namespace tabs
