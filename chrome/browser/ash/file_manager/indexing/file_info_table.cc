// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/file_info_table.h"

#include "sql/statement.h"

namespace file_manager {

namespace {

// The statement used to create the file_info table.
static constexpr char kCreateFileInfoTableQuery[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS file_info_table("
      "url_id INTEGER PRIMARY KEY NOT NULL REFERENCES url_table(url_id),"
      "last_modified INTEGER NOT NULL,"
      "size INTEGER NOT NULL)";
// clang-format on

// The statement used to insert a new term into the table.
static constexpr char kInsertFileInfoQuery[] =
    // clang-format off
    "INSERT OR REPLACE INTO file_info_table(url_id, last_modified, size) "
    "VALUES (?, ?, ?)";
// clang-format on

// The statement used to delete a FileInfo from the database by URL ID.
static constexpr char kDeleteFileInfoQuery[] =
    // clang-format off
    "DELETE FROM file_info_table WHERE url_id = ?";
// clang-format on

// The statement used fetch the file info by the URL ID.
static constexpr char kGetFileInfoQuery[] =
    // clang-format off
    "SELECT last_modified, size FROM file_info_table WHERE url_id = ?";
// clang-format on

}  // namespace

FileInfoTable::FileInfoTable(sql::Database* db) : db_(db) {}
FileInfoTable::~FileInfoTable() = default;

bool FileInfoTable::Init() {
  if (!db_->is_open()) {
    LOG(WARNING) << "Faield to initialize file_info_table "
                 << "due to closed database";
    return false;
  }
  sql::Statement create_table(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateFileInfoTableQuery));
  DCHECK(create_table.is_valid()) << "Invalid create the table statement: \""
                                  << create_table.GetSQLStatement() << "\"";
  if (!create_table.Run()) {
    LOG(ERROR) << "Failed to create the table";
    return false;
  }
  return true;
}

int64_t FileInfoTable::GetFileInfo(int64_t url_id, FileInfo* info) {
  DCHECK(info);
  sql::Statement get_file_info(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetFileInfoQuery));
  DCHECK(get_file_info.is_valid()) << "Invalid get file info statement: \""
                                   << get_file_info.GetSQLStatement() << "\"";
  get_file_info.BindInt64(0, url_id);
  if (!get_file_info.Step()) {
    return false;
  }
  info->last_modified = get_file_info.ColumnTime(0);
  info->size = get_file_info.ColumnInt64(1);
  return url_id;
}

int64_t FileInfoTable::DeleteFileInfo(int64_t url_id) {
  sql::Statement delete_file_info(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteFileInfoQuery));
  DCHECK(delete_file_info.is_valid())
      << "Invalid get file info statement: \""
      << delete_file_info.GetSQLStatement() << "\"";
  delete_file_info.BindInt64(0, url_id);
  if (!delete_file_info.Run()) {
    return -1;
  }
  return url_id;
}

int64_t FileInfoTable::PutFileInfo(int64_t url_id, const FileInfo& info) {
  sql::Statement insert_file_info(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertFileInfoQuery));
  DCHECK(insert_file_info.is_valid())
      << "Invalid create the table statement: \""
      << insert_file_info.GetSQLStatement() << "\"";
  insert_file_info.BindInt64(0, url_id);
  insert_file_info.BindTime(1, info.last_modified);
  insert_file_info.BindInt64(2, info.size);
  if (!insert_file_info.Run()) {
    LOG(ERROR) << "Failed to insert file_info";
    return -1;
  }
  return url_id;
}

}  // namespace file_manager
