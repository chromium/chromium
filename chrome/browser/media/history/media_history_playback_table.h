// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_PLAYBACK_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_PLAYBACK_TABLE_H_

#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"
#include "url/gurl.h"

namespace base {
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace content {
struct MediaPlayerWatchTime;
}  // namespace content

namespace media_history {

class MediaHistoryPlaybackTable : public MediaHistoryTableBase {
 public:
  struct MediaHistoryPlayback {
    MediaHistoryPlayback() = default;

    GURL url;
    base::TimeDelta watch_time;
    base::TimeDelta timestamp;
  };

  using MediaHistoryPlaybacks = std::vector<MediaHistoryPlayback>;

 private:
  friend class MediaHistoryStoreInternal;

  explicit MediaHistoryPlaybackTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  ~MediaHistoryPlaybackTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  // Returns a flag indicating whether the playback was created successfully.
  bool SavePlayback(const content::MediaPlayerWatchTime& watch_time);

  DISALLOW_COPY_AND_ASSIGN(MediaHistoryPlaybackTable);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_PLAYBACK_TABLE_H_
