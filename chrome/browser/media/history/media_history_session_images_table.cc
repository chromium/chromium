// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_session_images_table.h"

#include "base/strings/stringprintf.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_images_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "services/media_session/public/cpp/media_image.h"
#include "sql/statement.h"
#include "ui/gfx/geometry/size.h"

namespace media_history {

const char MediaHistorySessionImagesTable::kTableName[] = "sessionImage";

MediaHistorySessionImagesTable::MediaHistorySessionImagesTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistorySessionImagesTable::~MediaHistorySessionImagesTable() = default;

sql::InitStatus MediaHistorySessionImagesTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      base::StringPrintf("CREATE TABLE IF NOT EXISTS %s("
                         "session_id INTEGER NOT NULL,"
                         "image_id INTEGER NOT NULL,"
                         "width INTEGER,"
                         "height INTEGER, "
                         "CONSTRAINT fk_session "
                         "FOREIGN KEY (session_id) "
                         "REFERENCES %s(id) "
                         "ON DELETE CASCADE, "
                         "CONSTRAINT fk_image "
                         "FOREIGN KEY (image_id) "
                         "REFERENCES %s(id) "
                         "ON DELETE CASCADE "
                         ")",
                         kTableName, MediaHistorySessionTable::kTableName,
                         MediaHistoryImagesTable::kTableName)
          .c_str());

  if (success) {
    success = DB()->Execute(
        base::StringPrintf(
            "CREATE INDEX IF NOT EXISTS sessionImage_session_id_index ON "
            "%s (session_id)",
            kTableName)
            .c_str());
  }

  if (success) {
    success = DB()->Execute(
        base::StringPrintf(
            "CREATE INDEX IF NOT EXISTS sessionImage_image_id_index ON "
            "%s (image_id)",
            kTableName)
            .c_str());
  }

  if (success) {
    success = DB()->Execute(
        base::StringPrintf("CREATE UNIQUE INDEX IF NOT EXISTS "
                           "sessionImage_session_image_index ON "
                           "%s (session_id, image_id, width, height)",
                           kTableName)
            .c_str());
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history session images table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

bool MediaHistorySessionImagesTable::LinkImage(
    const int64_t session_id,
    const int64_t image_id,
    const absl::optional<gfx::Size> size) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  DCHECK(session_id);
  DCHECK(image_id);

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, base::StringPrintf("INSERT INTO %s "
                                        "(session_id, image_id, width, height) "
                                        "VALUES (?, ?, ?, ?)",
                                        kTableName)
                         .c_str()));
  statement.BindInt64(0, session_id);
  statement.BindInt64(1, image_id);

  if (size.has_value()) {
    statement.BindInt(2, size->width());
    statement.BindInt(3, size->height());
  } else {
    statement.BindNull(2);
    statement.BindNull(3);
  }

  return statement.Run();
}

std::vector<media_session::MediaImage>
MediaHistorySessionImagesTable::GetImagesForSession(const int64_t session_id) {
  std::vector<media_session::MediaImage> images;
  if (!CanAccessDatabase())
    return images;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "SELECT width, height, url, mime_type, image_id FROM %s "
          "INNER JOIN %s on %s.id = %s.image_id "
          "WHERE session_id = ? "
          "ORDER BY image_id ASC",
          kTableName, MediaHistoryImagesTable::kTableName,
          MediaHistoryImagesTable::kTableName, kTableName)
          .c_str()));
  statement.BindInt64(0, session_id);

  absl::optional<media_session::MediaImage> current;
  while (statement.Step()) {
    GURL url(statement.ColumnString(2));

    // If the current image does not have the same URL then it is a different
    // image and we should add it to the vector and reset it.
    if (current.has_value() && current->src != url) {
      images.push_back(*current);
      current.reset();
    }

    // If we don't have a current image then create one.
    if (!current.has_value()) {
      current = media_session::MediaImage();
      current->src = url;
      current->type = statement.ColumnString16(3);
    }

    // Add the size to the current image.
    if (statement.GetColumnType(0) == sql::ColumnType::kInteger &&
        statement.GetColumnType(1) == sql::ColumnType::kInteger) {
      current->sizes.push_back(
          gfx::Size(statement.ColumnInt(0), statement.ColumnInt(1)));
    }
  }

  if (current)
    images.push_back(*current);

  return images;
}

}  // namespace media_history
