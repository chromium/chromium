// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_playback_table.h"

#include "base/strings/stringprintf.h"
#include "base/updateable_sequenced_task_runner.h"
#include "content/public/browser/media_player_watch_time.h"
#include "sql/statement.h"

namespace media_history {

MediaHistoryPlaybackTable::MediaHistoryPlaybackTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryPlaybackTable::~MediaHistoryPlaybackTable() = default;

sql::InitStatus MediaHistoryPlaybackTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      "CREATE TABLE IF NOT EXISTS playback("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "origin_id INTEGER NOT NULL,"
      "url TEXT,"
      "timestamp_ms INTEGER,"
      "watch_time_ms INTEGER,"
      "has_video INTEGER,"
      "has_audio INTEGER,"
      "last_updated_time_s BIGINT NOT NULL,"
      "CONSTRAINT fk_origin "
      "FOREIGN KEY (origin_id) "
      "REFERENCES origin(id) "
      "ON DELETE CASCADE"
      ")");

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS origin_id_index ON "
        "playback (origin_id)");
  }

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS timestamp_index ON "
        "playback (timestamp_ms)");
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history playback table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

bool MediaHistoryPlaybackTable::SavePlayback(
    const content::MediaPlayerWatchTime& watch_time) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO playback "
      "(origin_id, url, watch_time_ms, timestamp_ms, has_video, has_audio, "
      "last_updated_time_s) "
      "VALUES ((SELECT id FROM origin WHERE origin = ?), ?, ?, ?, ?, ?, ?)"));
  statement.BindString(0, watch_time.origin.spec());
  statement.BindString(1, watch_time.url.spec());
  statement.BindInt(2, watch_time.cumulative_watch_time.InMilliseconds());
  statement.BindInt(3, watch_time.last_timestamp.InMilliseconds());
  statement.BindInt(4, watch_time.has_video);
  statement.BindInt(5, watch_time.has_audio);
  statement.BindInt64(6,
                      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  if (!statement.Run()) {
    return false;
  }

  return true;
}

}  // namespace media_history
