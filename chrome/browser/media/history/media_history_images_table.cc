// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_images_table.h"

#include "base/strings/stringprintf.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "sql/statement.h"

namespace media_history {

const char MediaHistoryImagesTable::kTableName[] = "mediaImage";

MediaHistoryImagesTable::MediaHistoryImagesTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryImagesTable::~MediaHistoryImagesTable() = default;

sql::InitStatus MediaHistoryImagesTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success =
      DB()->Execute(base::StringPrintf("CREATE TABLE IF NOT EXISTS %s("
                                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                       "playback_origin_id INTEGER NOT NULL,"
                                       "url TEXT NOT NULL,"
                                       "mime_type TEXT, "
                                       "CONSTRAINT fk_origin "
                                       "FOREIGN KEY (playback_origin_id) "
                                       "REFERENCES origin(id) "
                                       "ON DELETE CASCADE"
                                       ")",
                                       kTableName)
                        .c_str());

  if (success) {
    success = DB()->Execute(
        base::StringPrintf(
            "CREATE INDEX IF NOT EXISTS mediaImage_playback_origin_id_index ON "
            "%s (playback_origin_id)",
            kTableName)
            .c_str());
  }

  if (success) {
    success = DB()->Execute(
        base::StringPrintf("CREATE UNIQUE INDEX IF NOT EXISTS "
                           "mediaImage_playback_origin_id_url_index ON "
                           "%s (playback_origin_id, url)",
                           kTableName)
            .c_str());
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history images table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

absl::optional<int64_t> MediaHistoryImagesTable::SaveOrGetImage(
    const GURL& url,
    const url::Origin& playback_origin,
    const std::u16string& mime_type) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return absl::nullopt;

  {
    // First we should try and save the image in the database. It will not save
    // if we already have this image in the DB.
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        base::StringPrintf("INSERT OR IGNORE INTO %s "
                           "(playback_origin_id, url, mime_type) VALUES "
                           "((SELECT id FROM origin WHERE origin = ?), ?, ?)",
                           kTableName)
            .c_str()));
    statement.BindString(
        0, MediaHistoryOriginTable::GetOriginForStorage(playback_origin));
    statement.BindString(1, url.spec());
    statement.BindString16(2, mime_type);

    if (!statement.Run())
      return absl::nullopt;
  }

  // If the insert is successful and we have store an image row then we should
  // return the last insert id.
  if (DB()->GetLastChangeCount() == 1) {
    auto id = DB()->GetLastInsertRowId();
    if (id)
      return id;

    NOTREACHED();
  }

  DCHECK_EQ(0, DB()->GetLastChangeCount());

  {
    // If we did not save the image then we need to find the ID of the image.
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        base::StringPrintf(
            "SELECT id FROM %s WHERE playback_origin_id = (SELECT id FROM "
            "origin WHERE origin = ?) AND url = ?",
            kTableName)
            .c_str()));
    statement.BindString(
        0, MediaHistoryOriginTable::GetOriginForStorage(playback_origin));
    statement.BindString(1, url.spec());

    while (statement.Step()) {
      return statement.ColumnInt64(0);
    }
  }

  NOTREACHED_NORETURN();
}

}  // namespace media_history
