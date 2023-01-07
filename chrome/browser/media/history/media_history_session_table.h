// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_SESSION_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_SESSION_TABLE_H_

#include "chrome/browser/media/history/media_history_store.h"
#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace media_session {
struct MediaMetadata;
struct MediaPosition;
}  // namespace media_session

namespace url {
class Origin;
}  // namespace url

namespace media_history {

class MediaHistorySessionTable : public MediaHistoryTableBase {
 public:
  static const char kTableName[];

  bool DeleteURL(const GURL& url) override;

 private:
  friend class MediaHistoryStore;

  explicit MediaHistorySessionTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  MediaHistorySessionTable(const MediaHistorySessionTable&) = delete;
  MediaHistorySessionTable& operator=(const MediaHistorySessionTable&) = delete;
  ~MediaHistorySessionTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  // Returns the ID of the session if it was created successfully.
  absl::optional<int64_t> SavePlaybackSession(
      const GURL& url,
      const url::Origin& origin,
      const media_session::MediaMetadata& metadata,
      const absl::optional<media_session::MediaPosition>& position);

  std::vector<mojom::MediaHistoryPlaybackSessionRowPtr> GetPlaybackSessions(
      absl::optional<unsigned int> num_sessions,
      absl::optional<MediaHistoryStore::GetPlaybackSessionsFilter> filter);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_SESSION_TABLE_H_
