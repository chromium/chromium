// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_kaleidoscope_data_table.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "sql/statement.h"

namespace media_history {

const char MediaHistoryKaleidoscopeDataTable::kTableName[] = "kaleidoscopeData";

MediaHistoryKaleidoscopeDataTable::MediaHistoryKaleidoscopeDataTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryKaleidoscopeDataTable::~MediaHistoryKaleidoscopeDataTable() =
    default;

sql::InitStatus MediaHistoryKaleidoscopeDataTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      "CREATE TABLE IF NOT EXISTS kaleidoscopeData("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "data BLOB, "
      "result INTEGER, "
      "gaia_id TEXT UNIQUE, "
      "last_updated_time_s INTEGER)");

  if (!success) {
    ResetDB();
    DLOG(ERROR) << "Failed to create media history kaleidoscope data table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

bool MediaHistoryKaleidoscopeDataTable::Set(
    media::mojom::GetCollectionsResponsePtr data,
    const std::string& gaia_id) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO kaleidoscopeData "
      "(id, data, result, gaia_id, last_updated_time_s) VALUES "
      "(0, ?, ?, ?, ?)"));
  statement.BindBlob(0, data->response.data(), data->response.length());
  statement.BindInt64(1, static_cast<int>(data->result));
  statement.BindString(2, gaia_id);
  statement.BindInt64(3,
                      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());

  if (!statement.Run()) {
    DLOG(ERROR) << "Failed to update the data.";
    return false;
  }

  return true;
}

media::mojom::GetCollectionsResponsePtr MediaHistoryKaleidoscopeDataTable::Get(
    const std::string& gaia_id) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return nullptr;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT data, result, gaia_id, last_updated_time_s FROM "
      "kaleidoscopeData WHERE id = 0"));

  while (statement.Step()) {
    // If the GAIA id for the current user does not match the one we stored then
    // wipe the stored data and return an empty string.
    if (statement.ColumnString(2) != gaia_id) {
      CHECK(Delete());
      return nullptr;
    }

    // If the data that was stored was older than 24 hours then we should wipe
    // the stored data and return an empty string.
    auto updated_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(statement.ColumnInt64(3)));
    if ((base::Time::Now() - updated_time) > base::TimeDelta::FromDays(1)) {
      CHECK(Delete());
      return nullptr;
    }

    auto out = media::mojom::GetCollectionsResponse::New();
    statement.ColumnBlobAsString(0, &out->response);
    out->result = static_cast<media::mojom::GetCollectionsResult>(
        statement.ColumnInt64(1));
    return out;
  }

  // If there is no data then return nullptr.
  return nullptr;
}

bool MediaHistoryKaleidoscopeDataTable::Delete() {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(
      DB()->GetCachedStatement(SQL_FROM_HERE, "DELETE FROM kaleidoscopeData"));
  return statement.Run();
}

}  // namespace media_history
