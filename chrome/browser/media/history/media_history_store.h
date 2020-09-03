// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_STORE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_STORE_H_

#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/synchronization/atomic_flag.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_store.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/media_player_watch_time.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "url/origin.h"

namespace media_session {
struct MediaImage;
struct MediaMetadata;
struct MediaPosition;
}  // namespace media_session

namespace url {
class Origin;
}  // namespace url

namespace media_history {

class MediaHistoryOriginTable;
class MediaHistoryPlaybackTable;
class MediaHistorySessionTable;
class MediaHistorySessionImagesTable;
class MediaHistoryImagesTable;
class MediaHistoryFeedsTable;
class MediaHistoryFeedItemsTable;
class MediaHistoryKaleidoscopeDataTable;

// Refcounted as it is created, initialized and destroyed on a different thread
// from the DB sequence provided to the constructor of this class that is
// required for all methods performing database access.
class MediaHistoryStore : public base::RefCountedThreadSafe<MediaHistoryStore> {
 public:
  MediaHistoryStore(
      Profile* profile,
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  MediaHistoryStore(const MediaHistoryStore& t) = delete;
  MediaHistoryStore& operator=(const MediaHistoryStore&) = delete;

  using GetPlaybackSessionsFilter =
      base::RepeatingCallback<bool(const base::TimeDelta& duration,
                                   const base::TimeDelta& position)>;

  static const char kInitResultHistogramName[];
  static const char kInitResultAfterDeleteHistogramName[];
  static const char kPlaybackWriteResultHistogramName[];
  static const char kSessionWriteResultHistogramName[];
  static const char kDatabaseSizeKbHistogramName[];

  // When we initialize the database we store the result in
  // |kInitResultHistogramName|. Do not change the numbering since this
  // is recorded.
  enum class InitResult {
    kSuccess = 0,
    kFailedNoForeignKeys = 1,
    kFailedDatabaseTooNew = 2,
    kFailedInitializeTables = 3,
    kFailedToCreateDirectory = 4,
    kFailedToOpenDatabase = 5,
    kFailedToEstablishTransaction = 6,
    kFailedToCreateMetaTable = 7,
    kFailedToCommitTransaction = 8,
    kFailedToDeleteOldDatabase = 9,
    kMaxValue = kFailedToDeleteOldDatabase,
  };

  // If we write a playback into the database then we record the result to
  // |kPlaybackWriteResultHistogramName|. Do not change the numbering since this
  // is recorded.
  enum class PlaybackWriteResult {
    kSuccess = 0,
    kFailedToEstablishTransaction = 1,
    kFailedToWriteOrigin = 2,
    kFailedToWritePlayback = 3,
    kFailedToIncrementAggreatedWatchtime = 4,
    kFailedToWriteBadOrigin = 5,
    kMaxValue = kFailedToWriteBadOrigin,
  };

  // If we write a session into the database then we record the result to
  // |kSessionWriteResultHistogramName|. Do not change the numbering since this
  // is recorded.
  enum class SessionWriteResult {
    kSuccess = 0,
    kFailedToEstablishTransaction = 1,
    kFailedToWriteOrigin = 2,
    kFailedToWriteSession = 3,
    kFailedToWriteImage = 4,
    kMaxValue = kFailedToWriteImage,
  };

 protected:
  friend class MediaHistoryKeyedService;

  // Opens the database file from the |db_path|. Separated from the
  // constructor to ease construction/destruction of this object on one thread
  // and database access on the DB sequence of |db_task_runner_|. If
  // |should_reset| is true then this will delete and reset the DB.
  void Initialize(const bool should_reset);

  InitResult InitializeInternal();
  sql::InitStatus CreateOrUpgradeIfNeeded();
  sql::InitStatus InitializeTables();
  sql::Database* DB();

  // Returns a flag indicating whether the origin id was created successfully.
  bool CreateOriginId(const url::Origin& origin);

  void SavePlayback(const content::MediaPlayerWatchTime& watch_time);

  mojom::MediaHistoryStatsPtr GetMediaHistoryStats();
  int GetTableRowCount(const std::string& table_name);

  std::vector<mojom::MediaHistoryOriginRowPtr> GetOriginRowsForDebug();

  std::vector<mojom::MediaHistoryPlaybackRowPtr>
  GetMediaHistoryPlaybackRowsForDebug();

  std::vector<media_feeds::mojom::MediaFeedItemPtr> GetMediaFeedItems(
      const MediaHistoryKeyedService::GetMediaFeedItemsRequest& request);

  std::vector<media_feeds::mojom::MediaFeedPtr> GetMediaFeeds(
      const MediaHistoryKeyedService::GetMediaFeedsRequest& request);

  std::vector<url::Origin> GetHighWatchTimeOrigins(
      const base::TimeDelta& audio_video_watchtime_min);

  void SavePlaybackSession(
      const GURL& url,
      const media_session::MediaMetadata& metadata,
      const base::Optional<media_session::MediaPosition>& position,
      const std::vector<media_session::MediaImage>& artwork);

  std::vector<mojom::MediaHistoryPlaybackSessionRowPtr> GetPlaybackSessions(
      base::Optional<unsigned int> num_sessions,
      base::Optional<MediaHistoryStore::GetPlaybackSessionsFilter> filter);

  void DeleteAllOriginData(const std::set<url::Origin>& origins);
  void DeleteAllURLData(const std::set<GURL>& urls);

  std::set<GURL> GetURLsInTableForTest(const std::string& table);

  void DiscoverMediaFeed(const GURL& url);

  void StoreMediaFeedFetchResult(
      MediaHistoryKeyedService::MediaFeedFetchResult result);

  MediaHistoryKeyedService::PendingSafeSearchCheckList
  GetPendingSafeSearchCheckMediaFeedItems();

  void StoreMediaFeedItemSafeSearchResults(
      std::map<MediaHistoryKeyedService::SafeSearchID,
               media_feeds::mojom::SafeSearchResult> results);

  void UpdateMediaFeedDisplayTime(const int64_t feed_id);

  void ResetMediaFeed(const url::Origin& origin,
                      media_feeds::mojom::ResetReason reason);

  void ResetMediaFeedDueToCookies(const url::Origin& origin,
                                  const bool include_subdomains,
                                  const std::string& name,
                                  const net::CookieChangeCause& cause);

  void ResetMediaFeedDueToCacheClearing(
      const base::Time& start_time,
      const base::Time& end_time,
      MediaHistoryKeyedService::CacheClearingFilter filter);

  bool ResetMediaFeedInternal(const std::set<int64_t>& feed_ids,
                              media_feeds::mojom::ResetReason reason);

  // Cancels pending DB transactions. Should only be called on the UI thread.
  void SetCancelled();

  void IncrementMediaFeedItemsShownCount(const std::set<int64_t> feed_item_ids);

  void MarkMediaFeedItemAsClicked(const int64_t& feed_item_id);

  void DeleteMediaFeed(const int64_t feed_id);

  base::Optional<MediaHistoryKeyedService::MediaFeedFetchDetails>
  GetMediaFeedFetchDetails(const int64_t feed_id);

  void UpdateFeedUserStatus(const int64_t feed_id,
                            media_feeds::mojom::FeedUserStatus status);

  void SetKaleidoscopeData(media::mojom::GetCollectionsResponsePtr data,
                           const std::string& gaia_id);

  media::mojom::GetCollectionsResponsePtr GetKaleidoscopeData(
      const std::string& gaia_id);

  void DeleteKaleidoscopeData();

 private:
  friend class base::RefCountedThreadSafe<MediaHistoryStore>;

  ~MediaHistoryStore();

  void StoreMediaFeedFetchResultInternal(
      MediaHistoryKeyedService::MediaFeedFetchResult result);

  bool CanAccessDatabase() const;
  bool IsCancelled() const;

  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;
  const base::FilePath db_path_;
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<sql::MetaTable> meta_table_;
  scoped_refptr<MediaHistoryOriginTable> origin_table_;
  scoped_refptr<MediaHistoryPlaybackTable> playback_table_;
  scoped_refptr<MediaHistorySessionTable> session_table_;
  scoped_refptr<MediaHistorySessionImagesTable> session_images_table_;
  scoped_refptr<MediaHistoryImagesTable> images_table_;
  scoped_refptr<MediaHistoryFeedsTable> feeds_table_;
  scoped_refptr<MediaHistoryFeedItemsTable> feed_items_table_;
  scoped_refptr<MediaHistoryKaleidoscopeDataTable> kaleidoscope_table_;
  bool initialization_successful_;
  base::AtomicFlag cancelled_;
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_STORE_H_
