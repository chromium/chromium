// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_ENGAGEMENT_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_ENGAGEMENT_TABLE_H_

#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"
#include "url/origin.h"

namespace media_history {

class MediaHistoryEngagementTable : public MediaHistoryTableBase {
 public:
  struct MediaEngagementScore {
    MediaEngagementScore();

    url::Origin& origin;
    base::TimeDelta last_updated;  // timestamp (epoch seconds)
    int visits;
    int playbacks;
    base::TimeDelta last_playback_time;
    bool has_high_score;
  };

 private:
  friend class MediaHistoryStoreInternal;

  explicit MediaHistoryEngagementTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  ~MediaHistoryEngagementTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  DISALLOW_COPY_AND_ASSIGN(MediaHistoryEngagementTable);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_ENGAGEMENT_TABLE_H_
