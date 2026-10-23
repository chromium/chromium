// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/storage/memory_bank_table.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/database_utils/url_converter.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/table_management_helpers.h"
#include "sql/transaction.h"
#include "url/gurl.h"

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
constexpr std::string_view kNoteColumn = "note";
constexpr std::string_view kCollectionColumn = "collection";
constexpr std::string_view kImageUrlColumn = "image_url";

constexpr char kMemoryBankEmbeddingsTable[] = "memory_bank_embeddings";
constexpr char kEntryIdColumn[] = "entry_id";
constexpr char kEmbeddingBlobColumn[] = "embedding_blob";

constexpr char kMemoryBankEmbeddingsMetaTable[] = "memory_bank_embeddings_meta";
constexpr char kMetaNameColumn[] = "name";
constexpr char kMetaValueColumn[] = "value";
constexpr char kEmbeddingsVersionName[] = "embeddings_version";

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

  if (statement.GetColumnType(7) != sql::ColumnType::kNull) {
    entry.note = statement.ColumnString(7);
  }
  if (statement.GetColumnType(8) != sql::ColumnType::kNull) {
    entry.collection = statement.ColumnString(8);
  }
  if (statement.GetColumnType(9) != sql::ColumnType::kNull) {
    entry.image_url = GURL(statement.ColumnString(9));
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

bool MemoryBankTable::MigrateToVersion2AddNoteAndCollectionColumns() {
  if (!db_) {
    return false;
  }

  if (!sql::AddColumn(*db_, kMemoryBankEntriesTable, kNoteColumn, "TEXT")) {
    return false;
  }

  return sql::AddColumn(*db_, kMemoryBankEntriesTable, kCollectionColumn,
                        "TEXT");
}

bool MemoryBankTable::MigrateToVersion3AddImageUrlColumn() {
  if (!db_) {
    return false;
  }

  return sql::AddColumn(*db_, kMemoryBankEntriesTable, kImageUrlColumn, "TEXT");
}

bool MemoryBankTable::MigrateToVersion4AddEmbeddingsTables() {
  if (!db_) {
    return false;
  }

  if (!sql::CreateTable(*db_, kMemoryBankEmbeddingsTable,
                        /*column_names_and_types=*/
                        {
                            {kEntryIdColumn, "INTEGER PRIMARY KEY NOT NULL"},
                            {kEmbeddingBlobColumn, "BLOB NOT NULL"},
                        })) {
    return false;
  }

  return sql::CreateTable(*db_, kMemoryBankEmbeddingsMetaTable,
                          /*column_names_and_types=*/
                          {
                              {kMetaNameColumn, "TEXT PRIMARY KEY NOT NULL"},
                              {kMetaValueColumn, "INTEGER NOT NULL"},
                          });
}

std::optional<int64_t> MemoryBankTable::AddOrUpdateEntry(
    const MemoryBankEntry& entry) {
  if (!db_) {
    return std::nullopt;
  }

  sql::Statement statement;
  sql::CachedInsertBuilder(
      SQL_FROM_HERE, *db_, statement, kMemoryBankEntriesTable,
      /*column_names=*/
      {kIdColumn, kTypeColumn, kTimestampColumn, kUrlColumn, kTabTitleColumn,
       kSelectedTextColumn, kTagsColumn, kNoteColumn, kCollectionColumn,
       kImageUrlColumn},
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
  if (entry.note.has_value() && !entry.note->empty()) {
    statement.BindString(7, *entry.note);
  } else {
    statement.BindNull(7);
  }
  if (entry.collection.has_value() && !entry.collection->empty()) {
    statement.BindString(8, *entry.collection);
  } else {
    statement.BindNull(8);
  }
  if (entry.image_url.has_value() && entry.image_url->is_valid()) {
    statement.BindString(9,
                         database_utils::GurlToDatabaseUrl(*entry.image_url));
  } else {
    statement.BindNull(9);
  }

  if (!statement.Run()) {
    return std::nullopt;
  }
  return entry.id > 0 ? entry.id : db_->GetLastInsertRowId();
}

bool MemoryBankTable::UpdateEntryAnnotations(
    int64_t id,
    const std::vector<std::string>& tags,
    const std::optional<std::string>& note,
    const std::optional<std::string>& collection) {
  if (!db_) {
    return false;
  }

  static constexpr char kQuery[] =
      "UPDATE memory_bank_entries SET tags = ?, note = ?, collection = ? "
      "WHERE id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kQuery));

  if (!tags.empty()) {
    base::ListValue tags_list;
    for (const auto& tag : tags) {
      tags_list.Append(tag);
    }
    std::string tags_json = base::WriteJson(tags_list).value_or("");
    statement.BindString(0, tags_json);
  } else {
    statement.BindNull(0);
  }

  if (note.has_value() && !note->empty()) {
    statement.BindString(1, *note);
  } else {
    statement.BindNull(1);
  }

  if (collection.has_value() && !collection->empty()) {
    statement.BindString(2, *collection);
  } else {
    statement.BindNull(2);
  }

  statement.BindInt64(3, id);

  return statement.Run() && db_->GetLastChangeCount() > 0;
}

bool MemoryBankTable::AddOrUpdateEmbeddings(
    int64_t entry_id,
    base::span<const passage_embeddings::Embedding> embeddings) {
  if (!db_ || entry_id <= 0 || embeddings.empty()) {
    return false;
  }

  // All embeddings for an entry come from the same model, so they have the
  // same length (`dimension`). Pack `dimension` as a 4-byte native-endian
  // uint32 header, followed by each vector's floats back to back.
  const size_t dimension = embeddings.front().GetData().size();
  const bool dimensions_match = std::ranges::all_of(
      embeddings, [dimension](const passage_embeddings::Embedding& embedding) {
        return embedding.GetData().size() == dimension;
      });
  if (dimension == 0 || !dimensions_match ||
      !base::IsValueInRangeForNumericType<uint32_t>(dimension)) {
    return false;
  }

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(uint32_t) +
               embeddings.size() * dimension * sizeof(float));
  base::Extend(blob, base::U32ToNativeEndian(static_cast<uint32_t>(dimension)));
  for (const passage_embeddings::Embedding& embedding : embeddings) {
    base::Extend(blob, base::as_byte_span(base::allow_nonunique_obj,
                                          embedding.GetData()));
  }

  static constexpr char kQuery[] =
      "INSERT OR REPLACE INTO memory_bank_embeddings "
      "(entry_id, embedding_blob) "
      "SELECT id, ? FROM memory_bank_entries WHERE id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kQuery));
  statement.BindBlob(0, std::move(blob));
  statement.BindInt64(1, entry_id);

  return statement.Run() && db_->GetLastChangeCount() > 0;
}

std::vector<int64_t> MemoryBankTable::GetEntryIdsWithoutEmbeddings() {
  if (!db_) {
    return {};
  }

  static constexpr char kQuery[] =
      "SELECT id FROM memory_bank_entries "
      "WHERE id NOT IN (SELECT entry_id FROM memory_bank_embeddings)";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kQuery));

  std::vector<int64_t> ids;
  while (statement.Step()) {
    ids.push_back(statement.ColumnInt64(0));
  }
  return ids;
}

std::optional<int64_t> MemoryBankTable::GetEmbeddingsVersion() {
  if (!db_) {
    return std::nullopt;
  }

  sql::Statement statement;
  sql::CachedSelectBuilder(
      SQL_FROM_HERE, *db_, statement, kMemoryBankEmbeddingsMetaTable,
      /*columns=*/{kMetaValueColumn},
      /*modifiers=*/base::StrCat({"WHERE ", kMetaNameColumn, " = ?"}));
  statement.BindString(0, kEmbeddingsVersionName);

  if (!statement.Step()) {
    return std::nullopt;
  }
  return statement.ColumnInt64(0);
}

bool MemoryBankTable::SetEmbeddingsVersion(int64_t version) {
  if (!db_ || version <= 0) {
    return false;
  }

  if (GetEmbeddingsVersion() == version) {
    return true;
  }

  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  // Embeddings computed under a different embedder model version are not
  // comparable, so drop them all. They will be reported by
  // `GetEntryIdsWithoutEmbeddings()` for recomputation.
  sql::Statement delete_statement;
  sql::CachedDeleteBuilder(SQL_FROM_HERE, *db_, delete_statement,
                           kMemoryBankEmbeddingsTable);
  if (!delete_statement.Run()) {
    return false;
  }

  sql::Statement statement;
  sql::CachedInsertBuilder(SQL_FROM_HERE, *db_, statement,
                           kMemoryBankEmbeddingsMetaTable,
                           /*column_names=*/{kMetaNameColumn, kMetaValueColumn},
                           /*or_replace=*/true);
  statement.BindString(0, kEmbeddingsVersionName);
  statement.BindInt64(1, version);
  if (!statement.Run()) {
    return false;
  }

  return transaction.Commit();
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
       kSelectedTextColumn, kTagsColumn, kNoteColumn, kCollectionColumn,
       kImageUrlColumn},
      /*modifiers=*/"WHERE id = ?");
  statement.BindInt64(0, id);

  if (!statement.Step()) {
    return std::nullopt;
  }

  return ToMemoryBankEntry(statement);
}

std::vector<MemoryBankEntry> MemoryBankTable::GetEntriesByIds(
    base::span<const int64_t> ids) {
  if (!db_ || ids.empty()) {
    return {};
  }
  std::vector<MemoryBankEntry> entries;
  for (int64_t id : ids) {
    auto entry = GetEntry(id);
    if (entry.has_value()) {
      entries.push_back(std::move(*entry));
    }
  }
  return entries;
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
       kSelectedTextColumn, kTagsColumn, kNoteColumn, kCollectionColumn,
       kImageUrlColumn},
      /*modifiers=*/"ORDER BY timestamp DESC");

  std::vector<MemoryBankEntry> entries;
  while (statement.Step()) {
    entries.push_back(ToMemoryBankEntry(statement));
  }

  return entries;
}

std::vector<std::string> MemoryBankTable::GetAllTags() {
  if (!db_) {
    return {};
  }

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT DISTINCT tags FROM memory_bank_entries WHERE tags IS NOT NULL "
      "AND tags != ''"));

  std::set<std::string> unique_tags;
  while (statement.Step()) {
    std::string tags_json = statement.ColumnString(0);
    std::optional<base::ListValue> list_opt =
        base::JSONReader::ReadList(tags_json, base::JSON_PARSE_RFC);
    if (list_opt.has_value()) {
      for (const auto& val : list_opt.value()) {
        if (val.is_string()) {
          unique_tags.insert(val.GetString());
        }
      }
    }
  }

  return std::vector<std::string>(unique_tags.begin(), unique_tags.end());
}

std::vector<std::string> MemoryBankTable::GetAllCollections() {
  if (!db_) {
    return {};
  }

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT DISTINCT collection FROM memory_bank_entries WHERE collection "
      "IS NOT NULL AND collection != '' ORDER BY collection ASC"));

  std::vector<std::string> collections;
  while (statement.Step()) {
    collections.push_back(statement.ColumnString(0));
  }

  return collections;
}

size_t MemoryBankTable::GetEntryCount() {
  if (!db_) {
    return 0;
  }

  sql::Statement statement;
  sql::CachedSelectBuilder(SQL_FROM_HERE, *db_, statement,
                           kMemoryBankEntriesTable, /*columns=*/{"COUNT(*)"});
  if (statement.Step()) {
    return static_cast<size_t>(statement.ColumnInt64(0));
  }
  return 0;
}

bool MemoryBankTable::DeleteEntries(base::span<const int64_t> ids) {
  if (!db_ || ids.empty()) {
    return true;
  }

  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement delete_entry;
  sql::CachedDeleteBuilder(SQL_FROM_HERE, *db_, delete_entry,
                           kMemoryBankEntriesTable,
                           /*where_clause=*/base::StrCat({kIdColumn, " = ?"}));

  // Foreign keys are not used in //sql, so the embeddings row must be removed
  // explicitly alongside its entry.
  sql::Statement delete_embedding;
  sql::CachedDeleteBuilder(
      SQL_FROM_HERE, *db_, delete_embedding, kMemoryBankEmbeddingsTable,
      /*where_clause=*/base::StrCat({kEntryIdColumn, " = ?"}));

  for (int64_t id : ids) {
    delete_entry.Reset(/*clear_bound_vars=*/true);
    delete_entry.BindInt64(0, id);
    if (!delete_entry.Run()) {
      return false;
    }

    delete_embedding.Reset(/*clear_bound_vars=*/true);
    delete_embedding.BindInt64(0, id);
    if (!delete_embedding.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

}  // namespace context_hub
