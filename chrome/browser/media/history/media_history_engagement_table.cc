// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_engagement_table.h"

#include "base/strings/stringprintf.h"
#include "sql/statement.h"

namespace media_history {

MediaHistoryEngagementTable::MediaHistoryEngagementTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryEngagementTable::~MediaHistoryEngagementTable() = default;

sql::InitStatus MediaHistoryEngagementTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      "CREATE TABLE IF NOT EXISTS mediaEngagement("
      "origin_id INTEGER PRIMARY KEY,"
      "last_updated INTEGER,"
      "visits INTEGER,"
      "playbacks INTEGER,"
      "last_playback_time REAL,"
      "has_high_score INTEGER,"
      "CONSTRAINT fk_origin "
      "FOREIGN KEY (origin_id) "
      "REFERENCES origin(id) "
      "ON DELETE CASCADE"
      ")");

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history engagement table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

}  // namespace media_history