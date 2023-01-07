// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_IMAGES_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_IMAGES_TABLE_H_

#include "base/strings/string_util.h"
#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace media_history {

class MediaHistoryImagesTable : public MediaHistoryTableBase {
 public:
  static const char kTableName[];

 private:
  friend class MediaHistoryStore;

  explicit MediaHistoryImagesTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  MediaHistoryImagesTable(const MediaHistoryImagesTable&) = delete;
  MediaHistoryImagesTable& operator=(const MediaHistoryImagesTable&) = delete;
  ~MediaHistoryImagesTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  // Saves the image or gets the image ID if it is already in the database.
  absl::optional<int64_t> SaveOrGetImage(const GURL& url,
                                         const url::Origin& playback_origin,
                                         const std::u16string& mime_type);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_IMAGES_TABLE_H_
