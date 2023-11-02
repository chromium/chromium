// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_SESSION_IMAGES_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_SESSION_IMAGES_TABLE_H_

#include <vector>

#include "base/strings/string_util.h"
#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace media_session {
struct MediaImage;
}  // namespace media_session

namespace gfx {
class Size;
}  // namespace gfx

namespace media_history {

class MediaHistorySessionImagesTable : public MediaHistoryTableBase {
 public:
  static const char kTableName[];

 private:
  friend class MediaHistoryStore;

  explicit MediaHistorySessionImagesTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  MediaHistorySessionImagesTable(const MediaHistorySessionImagesTable&) =
      delete;
  MediaHistorySessionImagesTable& operator=(
      const MediaHistorySessionImagesTable&) = delete;
  ~MediaHistorySessionImagesTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  // Saves the link and returns whether it was successful.
  bool LinkImage(const int64_t session_id,
                 const int64_t image_id,
                 const absl::optional<gfx::Size> size);

  // Gets all the images for a session.
  std::vector<media_session::MediaImage> GetImagesForSession(
      const int64_t session_id);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_SESSION_IMAGES_TABLE_H_
