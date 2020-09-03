// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KALEIDOSCOPE_DATA_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KALEIDOSCOPE_DATA_TABLE_H_

#include <string>

#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/history/media_history_table_base.h"
#include "chrome/browser/media/kaleidoscope/mojom/kaleidoscope.mojom.h"
#include "sql/init_status.h"

namespace media_history {

class MediaHistoryKaleidoscopeDataTable : public MediaHistoryTableBase {
 public:
  static const char kTableName[];

 private:
  friend class MediaHistoryStore;

  explicit MediaHistoryKaleidoscopeDataTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  ~MediaHistoryKaleidoscopeDataTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  bool Set(media::mojom::GetCollectionsResponsePtr data,
           const std::string& gaia_id);

  media::mojom::GetCollectionsResponsePtr Get(const std::string& gaia_id);

  bool Delete();

  DISALLOW_COPY_AND_ASSIGN(MediaHistoryKaleidoscopeDataTable);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KALEIDOSCOPE_DATA_TABLE_H_
