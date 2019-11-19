// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_ORIGIN_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_ORIGIN_TABLE_H_

#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"

namespace media_history {

class MediaHistoryOriginTable : public MediaHistoryTableBase {
 private:
  friend class MediaHistoryStoreInternal;

  explicit MediaHistoryOriginTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  ~MediaHistoryOriginTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  // Returns a flag indicating whether the origin id was created successfully.
  bool CreateOriginId(const std::string& origin);

  DISALLOW_COPY_AND_ASSIGN(MediaHistoryOriginTable);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_ORIGIN_TABLE_H_
