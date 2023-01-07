// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_session_table.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "services/media_session/public/cpp/media_image.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/media_position.h"
#include "sql/statement.h"
#include "url/origin.h"

namespace media_history {

const char MediaHistorySessionTable::kTableName[] = "playbackSession";

MediaHistorySessionTable::MediaHistorySessionTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistorySessionTable::~MediaHistorySessionTable() = default;

sql::InitStatus MediaHistorySessionTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success =
      DB()->Execute(base::StringPrintf("CREATE TABLE IF NOT EXISTS %s("
                                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                       "origin_id INTEGER NOT NULL,"
                                       "url TEXT NOT NULL UNIQUE,"
                                       "duration_ms INTEGER,"
                                       "position_ms INTEGER,"
                                       "last_updated_time_s BIGINT NOT NULL,"
                                       "title TEXT, "
                                       "artist TEXT, "
                                       "album TEXT, "
                                       "source_title TEXT, "
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
            "CREATE INDEX IF NOT EXISTS playbackSession_origin_id_index ON "
            "%s (origin_id)",
            kTableName)
            .c_str());
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history playback session table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

absl::optional<int64_t> MediaHistorySessionTable::SavePlaybackSession(
    const GURL& url,
    const url::Origin& origin,
    const media_session::MediaMetadata& metadata,
    const absl::optional<media_session::MediaPosition>& position) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return absl::nullopt;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "REPLACE INTO %s "
          "(origin_id, url, duration_ms, position_ms, last_updated_time_s, "
          "title, artist, album, source_title) "
          "VALUES "
          "((SELECT id FROM origin WHERE origin = ?), ?, ?, ?, ?, ?, ?, ?, ?)",
          kTableName)
          .c_str()));
  statement.BindString(0, MediaHistoryOriginTable::GetOriginForStorage(origin));
  statement.BindString(1, url.spec());

  if (position.has_value()) {
    auto duration_ms = position->duration().InMilliseconds();
    auto position_ms = position->GetPosition().InMilliseconds();

    statement.BindInt64(2, duration_ms);
    statement.BindInt64(3, position_ms);
  } else {
    statement.BindInt64(2, 0);
    statement.BindInt64(3, 0);
  }

  statement.BindInt64(4,
                      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  statement.BindString16(5, metadata.title);
  statement.BindString16(6, metadata.artist);
  statement.BindString16(7, metadata.album);
  statement.BindString16(8, metadata.source_title);

  if (statement.Run()) {
    return DB()->GetLastInsertRowId();
  }

  return absl::nullopt;
}

std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>
MediaHistorySessionTable::GetPlaybackSessions(
    absl::optional<unsigned int> num_sessions,
    absl::optional<MediaHistoryStore::GetPlaybackSessionsFilter> filter) {
  std::vector<mojom::MediaHistoryPlaybackSessionRowPtr> sessions;
  if (!CanAccessDatabase())
    return sessions;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "SELECT id, url, duration_ms, position_ms, title, artist, "
          "album, source_title, last_updated_time_s FROM %s ORDER BY id DESC",
          kTableName)
          .c_str()));

  while (statement.Step()) {
    auto duration = base::Milliseconds(statement.ColumnInt64(2));
    auto position = base::Milliseconds(statement.ColumnInt64(3));

    // Skip any that should not be shown.
    if (filter.has_value() && !filter->Run(duration, position))
      continue;

    auto session(mojom::MediaHistoryPlaybackSessionRow::New());
    session->id = statement.ColumnInt64(0);
    session->url = GURL(statement.ColumnString(1));
    session->duration = duration;
    session->position = position;
    session->metadata.title = statement.ColumnString16(4);
    session->metadata.artist = statement.ColumnString16(5);
    session->metadata.album = statement.ColumnString16(6);
    session->metadata.source_title = statement.ColumnString16(7);
    session->last_updated_time = base::Time::FromDeltaSinceWindowsEpoch(
                                     base::Seconds(statement.ColumnInt64(8)))
                                     .ToJsTime();

    sessions.push_back(std::move(session));

    // If we have all the sessions we want we can stop loading data from the
    // database.
    if (num_sessions.has_value() && sessions.size() >= *num_sessions)
      break;
  }

  return sessions;
}

bool MediaHistorySessionTable::DeleteURL(const GURL& url) {
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM playbackSession WHERE url = ?"));
  statement.BindString(0, url.spec());
  return statement.Run();
}

}  // namespace media_history
