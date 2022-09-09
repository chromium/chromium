// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_ORIGIN_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_ORIGIN_TABLE_H_

#include <string>
#include <vector>

#include "base/task/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"

namespace url {
class Origin;
}  // namespace url

namespace media_history {

class MediaHistoryOriginTable : public MediaHistoryTableBase {
 public:
  static const char kTableName[];

  MediaHistoryOriginTable(const MediaHistoryOriginTable&) = delete;
  MediaHistoryOriginTable& operator=(const MediaHistoryOriginTable&) = delete;

  // Returns the origin as a string for storage.
  static std::string GetOriginForStorage(const url::Origin& origin);

 private:
  friend class MediaHistoryStore;

  explicit MediaHistoryOriginTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  ~MediaHistoryOriginTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  // Returns a flag indicating whether the origin id was created successfully.
  bool CreateOriginId(const url::Origin& origin);

  // Returns a flag indicating whether watchtime was increased successfully.
  bool IncrementAggregateAudioVideoWatchTime(const url::Origin& origin,
                                             const base::TimeDelta& time);

  // Recalculates the aggregate audio+video watchtime and returns a flag as to
  // whether this was successful.
  bool RecalculateAggregateAudioVideoWatchTime(const url::Origin& origin);

  // Deletes an origin from the database and returns a flag as to whether this
  // was successful.
  bool Delete(const url::Origin& origin);

  // Gets the origins which have watchtime above the given threshold.
  std::vector<url::Origin> GetHighWatchTimeOrigins(
      const base::TimeDelta& audio_video_watchtime_min);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_ORIGIN_TABLE_H_
