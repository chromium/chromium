// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_playback_table.h"

#include "base/strings/stringprintf.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "content/public/browser/media_player_watch_time.h"
#include "sql/statement.h"

namespace media_history {

const char MediaHistoryPlaybackTable::kTableName[] = "playback";

MediaHistoryPlaybackTable::MediaHistoryPlaybackTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryPlaybackTable::~MediaHistoryPlaybackTable() = default;

sql::InitStatus MediaHistoryPlaybackTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success =
      DB()->Execute(base::StringPrintf("CREATE TABLE IF NOT EXISTS %s("
                                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                       "origin_id INTEGER NOT NULL,"
                                       "url TEXT,"
                                       "watch_time_s INTEGER,"
                                       "has_video INTEGER,"
                                       "has_audio INTEGER,"
                                       "last_updated_time_s BIGINT NOT NULL,"
                                       "CONSTRAINT fk_origin "
                                       "FOREIGN KEY (origin_id) "
                                       "REFERENCES origin(id) "
                                       "ON DELETE CASCADE"
                                       ")",
                                       kTableName)
                        .c_str());

  if (success) {
    success = DB()->Execute(
        base::StringPrintf(
            "CREATE INDEX IF NOT EXISTS playback_origin_id_index ON "
            "%s (origin_id)",
            kTableName)
            .c_str());
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
      base::StringPrintf("INSERT INTO %s "
                         "(origin_id, url, watch_time_s, has_video, has_audio, "
                         "last_updated_time_s) "
                         "VALUES ((SELECT id FROM origin WHERE origin = ?), "
                         "?, ?, ?, ?, ?)",
                         kTableName)
          .c_str()));
  statement.BindString(0, MediaHistoryOriginTable::GetOriginForStorage(
                              url::Origin::Create(watch_time.origin)));
  statement.BindString(1, watch_time.url.spec());
  statement.BindInt64(2, watch_time.cumulative_watch_time.InSeconds());
  statement.BindInt(3, watch_time.has_video);
  statement.BindInt(4, watch_time.has_audio);
  statement.BindInt64(5,
                      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  if (!statement.Run()) {
    return false;
  }

  return true;
}

std::vector<mojom::MediaHistoryPlaybackRowPtr>
MediaHistoryPlaybackTable::GetPlaybackRows() {
  std::vector<mojom::MediaHistoryPlaybackRowPtr> playbacks;
  if (!CanAccessDatabase())
    return playbacks;

  sql::Statement statement(DB()->GetUniqueStatement(
      base::StringPrintf(
          "SELECT url, watch_time_s, has_audio, has_video, last_updated_time_s "
          "FROM %s",
          kTableName)
          .c_str()));

  while (statement.Step()) {
    mojom::MediaHistoryPlaybackRowPtr playback(
        mojom::MediaHistoryPlaybackRow::New());

    playback->url = GURL(statement.ColumnString(0));
    playback->watchtime = base::Seconds(statement.ColumnInt64(1));
    playback->has_audio = statement.ColumnBool(2);
    playback->has_video = statement.ColumnBool(3);
    playback->last_updated_time = base::Time::FromDeltaSinceWindowsEpoch(
                                      base::Seconds(statement.ColumnInt64(4)))
                                      .ToJsTime();

    playbacks.push_back(std::move(playback));
  }

  DCHECK(statement.Succeeded());
  return playbacks;
}

bool MediaHistoryPlaybackTable::DeleteURL(const GURL& url) {
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM playback WHERE url = ?"));
  statement.BindString(0, url.spec());
  return statement.Run();
}

}  // namespace media_history
