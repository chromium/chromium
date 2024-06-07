// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"

#include <memory>
#include <optional>
#include <sstream>

#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace ash::file_system_provider {

namespace {

static constexpr char kItemsCreateTableSql[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS items ("
        "id INTEGER NOT NULL UNIQUE, "
        "fsp_path TEXT NOT NULL, "
        "version_tag TEXT NOT NULL, "
        "accessed_time INTEGER NOT NULL, "
        "UNIQUE(fsp_path, version_tag) ON CONFLICT REPLACE, "
        "PRIMARY KEY(id AUTOINCREMENT))";
// clang-format on

static constexpr char kInsertItemSql[] =
    // clang-format off
    "INSERT INTO items "
    "(fsp_path, version_tag, accessed_time) VALUES (?, ?, ?) "
    "RETURNING id";
// clang-format on

static constexpr char kSelectItemByIdSql[] =
    // clang-format off
    "SELECT fsp_path, version_tag, accessed_time FROM items WHERE id=? LIMIT 1";
// clang-format on

static constexpr char kSelectAllItemsSql[] =
    // clang-format off
    "SELECT id, fsp_path, version_tag, accessed_time FROM items";
// clang-format on

static constexpr char kUpdateAccessedTimeByIdSql[] =
    // clang-format off
    "UPDATE items SET accessed_time = ? WHERE id = ?";
// clang-format on

}  // namespace

ContextDatabase::ContextDatabase(const base::FilePath& db_path)
    : db_path_(db_path), db_({sql::DatabaseOptions{}}) {
  // Can be constructed on any sequence, the first call to `Initialize` should
  // be made on the blocking task runner.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

// The current database version number.
constexpr int ContextDatabase::kCurrentVersionNumber = 1;

// The oldest version that is still compatible with `kCurrentVersionNumber`.
constexpr int ContextDatabase::kCompatibleVersionNumber = 1;

ContextDatabase::Item::Item(int64_t id,
                            const std::string& fsp_path,
                            const std::string& version_tag,
                            base::Time accessed_time)
    : id(id),
      fsp_path(fsp_path),
      version_tag(version_tag),
      accessed_time(accessed_time) {}

bool ContextDatabase::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_.set_histogram_tag("FSPContextDatabase");

  // TODO(b/332636364): Once the logic for the database has landed, let's stop
  // removing the database on every `Initialize` call.
  if (!Raze()) {
    LOG(ERROR) << "Failed to remove old database";
    return false;
  }

  DCHECK(!db_.is_open()) << "Database is already open";

  if (db_path_.empty() && !db_.OpenInMemory()) {
    LOG(ERROR) << "In memory database initialization failed";
    return false;
  } else if (!db_path_.empty() && !db_.Open(db_path_)) {
    LOG(ERROR) << "Initialization of '" << db_path_ << "' failed";
    Raze();
    return false;
  }

  sql::Transaction committer(&db_);
  if (!committer.Begin()) {
    LOG(ERROR) << "Can't start SQL transaction";
    return false;
  }

  if (!db_.Execute(kItemsCreateTableSql)) {
    LOG(ERROR) << "Can't setup items table";
    Raze();
    return false;
  }

  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    Raze();
    return false;
  }

  return committer.Commit();
}

bool ContextDatabase::AddItem(const base::FilePath& fsp_path,
                              const std::string& version_tag,
                              base::Time accessed_time,
                              int64_t* inserted_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (fsp_path.empty() || version_tag.empty() || accessed_time.is_null()) {
    return false;
  }

  std::unique_ptr<sql::Statement> statement = std::make_unique<sql::Statement>(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertItemSql));
  if (!statement) {
    LOG(ERROR) << "Couldn't create SQL statement";
    return false;
  }

  statement->BindString(0, fsp_path.value());
  statement->BindString(1, version_tag);
  statement->BindInt64(2, accessed_time.InMillisecondsSinceUnixEpoch());
  if (!statement->Step()) {
    LOG(ERROR) << "Couldn't execute statement";
    return false;
  }

  *inserted_id = statement->ColumnInt64(0);
  return true;
}

std::unique_ptr<std::optional<ContextDatabase::Item>>
ContextDatabase::GetItemById(int64_t item_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (item_id < 0) {
    return nullptr;
  }

  std::unique_ptr<sql::Statement> statement = std::make_unique<sql::Statement>(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectItemByIdSql));
  if (!statement) {
    LOG(ERROR) << "Couldn't create SQL statement";
    return nullptr;
  }

  statement->BindInt64(0, item_id);
  if (!statement->Step()) {
    // In the event the `Step()` failed, this could simply mean there is no item
    // for the `item_id`. `Succeeded` will return true in this case.
    if (statement->Succeeded()) {
      return std::make_unique<std::optional<Item>>(std::nullopt);
    }
    LOG(ERROR) << "Couldn't execute statement";
    return nullptr;
  }

  return std::make_unique<std::optional<ContextDatabase::Item>>(Item(
      item_id,
      /*fsp_path=*/statement->ColumnString(0),
      /*version_tag=*/statement->ColumnString(1),
      /*accessed_time=*/
      base::Time::FromMillisecondsSinceUnixEpoch(statement->ColumnInt64(2))));
}

bool ContextDatabase::UpdateAccessedTime(int64_t item_id,
                                         base::Time new_accessed_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<sql::Statement> statement = std::make_unique<sql::Statement>(
      db_.GetCachedStatement(SQL_FROM_HERE, kUpdateAccessedTimeByIdSql));
  if (!statement) {
    LOG(ERROR) << "Couldn't create SQL statement";
    return false;
  }

  statement->BindInt64(0, new_accessed_time.InMillisecondsSinceUnixEpoch());
  statement->BindInt64(1, item_id);
  return statement->Run();
}

ContextDatabase::IdToItemMap ContextDatabase::GetAllItems() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<sql::Statement> statement = std::make_unique<sql::Statement>(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectAllItemsSql));
  if (!statement) {
    LOG(ERROR) << "Couldn't create SQL statement";
    return {};
  }

  std::map<int64_t, Item> items;
  while (statement->Step()) {
    items.try_emplace(
        statement->ColumnInt64(0), statement->ColumnInt64(0),
        statement->ColumnString(1), statement->ColumnString(2),
        base::Time::FromMillisecondsSinceUnixEpoch(statement->ColumnInt64(3)));
  }

  if (!statement->Succeeded()) {
    return {};
  }

  return items;
}

bool ContextDatabase::RemoveItemsByIds(std::vector<int64_t> item_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::stringstream delete_in_clause;
  for (size_t i = 0; i < item_ids.size(); i++) {
    delete_in_clause << base::NumberToString(item_ids.at(i));
    if (i < item_ids.size() - 1) {
      delete_in_clause << "','";
    }
  }

  const std::string remove_items_by_id_sql = base::StrCat(
      {"DELETE FROM items WHERE id IN ('", delete_in_clause.str(), "')"});
  CHECK(db_.IsSQLValid(remove_items_by_id_sql));

  // TODO(b/341833149): Cache the statement.
  std::unique_ptr<sql::Statement> statement = std::make_unique<sql::Statement>(
      db_.GetUniqueStatement(remove_items_by_id_sql));
  if (!statement) {
    LOG(ERROR) << "Couldn't create SQL statement";
    return {};
  }

  return statement->Run();
}

base::WeakPtr<ContextDatabase> ContextDatabase::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool ContextDatabase::Raze() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  meta_table_.Reset();
  if (!db_.is_open()) {
    return true;
  }

  db_.Poison();
  return sql::Database::Delete(db_path_);
}

ContextDatabase::~ContextDatabase() = default;

}  // namespace ash::file_system_provider
