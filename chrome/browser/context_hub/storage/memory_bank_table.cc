// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/storage/memory_bank_table.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/database_utils/url_converter.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/table_management_helpers.h"
#include "sql/transaction.h"

namespace context_hub {

namespace {

constexpr std::string_view kMemoryBankEntriesTable = "memory_bank_entries";
constexpr std::string_view kIdColumn = "id";
constexpr std::string_view kTypeColumn = "type";
constexpr std::string_view kTimestampColumn = "timestamp";
constexpr std::string_view kUrlColumn = "url";
constexpr std::string_view kTabTitleColumn = "tab_title";
constexpr std::string_view kSelectedTextColumn = "selected_text";
constexpr std::string_view kTagsColumn = "tags";

MemoryBankEntry ToMemoryBankEntry(sql::Statement& statement) {
  MemoryBankEntry entry;
  entry.id = statement.ColumnInt64(0);
  entry.type = static_cast<MemoryBankType>(statement.ColumnInt(1));
  entry.timestamp = statement.ColumnTime(2);
  entry.url = GURL(statement.ColumnString(3));
  entry.tab_title = statement.ColumnString(4);
  if (statement.GetColumnType(5) != sql::ColumnType::kNull) {
    entry.selected_text = statement.ColumnString(5);
  }

  if (statement.GetColumnType(6) != sql::ColumnType::kNull) {
    std::string tags_json = statement.ColumnString(6);
    std::optional<base::ListValue> list_opt =
        base::JSONReader::ReadList(tags_json, base::JSON_PARSE_RFC);
    if (list_opt.has_value()) {
      for (const auto& val : list_opt.value()) {
        if (val.is_string()) {
          entry.tags.push_back(val.GetString());
        }
      }
    }
  }

  return entry;
}

}  // namespace

MemoryBankTable::MemoryBankTable() = default;
MemoryBankTable::~MemoryBankTable() = default;

bool MemoryBankTable::Init(sql::Database* db) {
  if (!db) {
    return false;
  }
  db_ = db;
  return true;
}

bool MemoryBankTable::MigrateFromCleanStateToVersion1() {
  if (!db_) {
    return false;
  }

  if (!sql::CreateTable(*db_, kMemoryBankEntriesTable,
                        /*column_names_and_types=*/
                        {
                            {kIdColumn, "INTEGER PRIMARY KEY"},
                            {kTypeColumn, "INTEGER NOT NULL"},
                            {kTimestampColumn, "INTEGER NOT NULL"},
                            {kUrlColumn, "TEXT NOT NULL"},
                            {kTabTitleColumn, "TEXT NOT NULL"},
                            {kSelectedTextColumn, "TEXT"},
                            {kTagsColumn, "TEXT"},
                        })) {
    return false;
  }

  if (!sql::CreateIndex(*db_, kMemoryBankEntriesTable, {kTimestampColumn})) {
    return false;
  }

  return sql::CreateIndex(*db_, kMemoryBankEntriesTable, {kUrlColumn});
}

bool MemoryBankTable::AddOrUpdateEntry(const MemoryBankEntry& entry) {
  if (!db_) {
    return false;
  }

  sql::Statement statement;
  sql::CachedInsertBuilder(
      SQL_FROM_HERE, *db_, statement, kMemoryBankEntriesTable,
      /*column_names=*/
      {kIdColumn, kTypeColumn, kTimestampColumn, kUrlColumn, kTabTitleColumn,
       kSelectedTextColumn, kTagsColumn},
      /*or_replace=*/true);

  if (entry.id > 0) {
    statement.BindInt64(0, entry.id);
  } else {
    statement.BindNull(0);
  }
  statement.BindInt(1, static_cast<int>(entry.type));
  statement.BindTime(2, entry.timestamp);
  statement.BindString(3, database_utils::GurlToDatabaseUrl(entry.url));
  statement.BindString(4, entry.tab_title);
  if (entry.selected_text.has_value()) {
    statement.BindString(5, *entry.selected_text);
  } else {
    statement.BindNull(5);
  }
  if (!entry.tags.empty()) {
    base::ListValue tags_list;
    for (const auto& tag : entry.tags) {
      tags_list.Append(tag);
    }
    std::string tags_json = base::WriteJson(tags_list).value_or("");
    statement.BindString(6, tags_json);
  } else {
    statement.BindNull(6);
  }
  return statement.Run();
}

std::optional<MemoryBankEntry> MemoryBankTable::GetEntry(int64_t id) {
  if (!db_) {
    return std::nullopt;
  }

  sql::Statement statement;
  sql::CachedSelectBuilder(
      SQL_FROM_HERE, *db_, statement, kMemoryBankEntriesTable,
      /*columns=*/
      {kIdColumn, kTypeColumn, kTimestampColumn, kUrlColumn, kTabTitleColumn,
       kSelectedTextColumn, kTagsColumn},
      /*modifiers=*/"WHERE id = ?");
  statement.BindInt64(0, id);

  if (!statement.Step()) {
    return std::nullopt;
  }

  return ToMemoryBankEntry(statement);
}

std::vector<MemoryBankEntry> MemoryBankTable::GetAllEntries() {
  if (!db_) {
    return {};
  }

  sql::Statement statement;
  sql::CachedSelectBuilder(
      SQL_FROM_HERE, *db_, statement, kMemoryBankEntriesTable,
      /*columns=*/
      {kIdColumn, kTypeColumn, kTimestampColumn, kUrlColumn, kTabTitleColumn,
       kSelectedTextColumn, kTagsColumn},
      /*modifiers=*/"ORDER BY timestamp DESC");

  std::vector<MemoryBankEntry> entries;
  while (statement.Step()) {
    entries.push_back(ToMemoryBankEntry(statement));
  }

  return entries;
}

bool MemoryBankTable::DeleteEntries(base::span<const int64_t> ids) {
  if (!db_ || ids.empty()) {
    return true;
  }

  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement statement;
  sql::CachedDeleteBuilder(SQL_FROM_HERE, *db_, statement,
                           kMemoryBankEntriesTable,
                           /*where_clause=*/base::StrCat({kIdColumn, " = ?"}));

  for (int64_t id : ids) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, id);
    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

}  // namespace context_hub
