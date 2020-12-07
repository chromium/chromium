// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEEDS_TABLE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEEDS_TABLE_H_

#include <vector>

#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_table_base.h"
#include "sql/init_status.h"
#include "url/gurl.h"

namespace base {
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace media_history {

class MediaHistoryFeedsTable : public MediaHistoryTableBase {
 public:
  static const char kTableName[];

  static const char kFeedReadResultHistogramName[];

  // If we read a feed item from the database then we record the result to
  // |kFeedReadResultHistogramName|. Do not change the numbering since this
  // is recorded.
  enum class FeedReadResult {
    kSuccess = 0,
    kBadUserStatus = 1,
    kBadFetchResult = 2,
    kBadLogo = 3,
    kBadUserIdentifier = 4,
    kBadResetReason = 5,
    kBadSafeSearchResult = 6,
    kMaxValue = kBadSafeSearchResult,
  };

 private:
  friend class MediaHistoryStore;

  explicit MediaHistoryFeedsTable(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  MediaHistoryFeedsTable(const MediaHistoryFeedsTable&) = delete;
  MediaHistoryFeedsTable& operator=(const MediaHistoryFeedsTable&) = delete;
  ~MediaHistoryFeedsTable() override;

  // MediaHistoryTableBase:
  sql::InitStatus CreateTableIfNonExistent() override;

  // Saves a newly discovered feed in the database.
  bool DiscoverFeed(const GURL& url, const base::Optional<GURL>& favicon);

  // Updates the feed following a fetch.
  bool UpdateFeedFromFetch(
      const int64_t feed_id,
      const media_feeds::mojom::FetchResult result,
      const bool was_fetched_from_cache,
      const int item_count,
      const int item_play_next_count,
      const int item_content_types,
      const std::vector<media_feeds::mojom::MediaImagePtr>& logos,
      const media_feeds::mojom::UserIdentifier* user_identifier,
      const std::string& display_name,
      const int item_safe_count,
      const std::string& cookie_name_filter);

  // Returns the feed rows in the database.
  std::vector<media_feeds::mojom::MediaFeedPtr> GetRows(
      const MediaHistoryKeyedService::GetMediaFeedsRequest& request);

  // Updates the display time to now and returns a boolean if it was saved.
  bool UpdateDisplayTime(const int64_t feed_id);

  // Recalculates the Safe Search safe item count and returns true if it was
  // successful.
  bool RecalculateSafeSearchItemCount(const int64_t feed_id);

  // Resets the feed to defaults and returns a boolean if it was saved.
  bool Reset(const int64_t feed_id,
             const media_feeds::mojom::ResetReason reason);

  // Deletes the feed with |feed_id| and returns a boolean if it was successful.
  bool Delete(const int64_t feed_id);

  // Returns the fetch details for the feed.
  base::Optional<MediaHistoryKeyedService::MediaFeedFetchDetails>
  GetFetchDetails(const int64_t feed_id);

  // Clears the reset reason for a feed and returns a boolean if it was saved.
  bool ClearResetReason(const int64_t feed_id);

  // Returns the cookie name filter for |feed_id| or an empty string.
  std::string GetCookieNameFilter(const int64_t feed_id);

  // Gets the feed for |origin|'s subdomains.
  std::set<int64_t> GetFeedsForOriginSubdomain(const url::Origin& origin);

  // Gets the feed for |origin|.
  base::Optional<int64_t> GetFeedForOrigin(const url::Origin& origin);

  // Returns all the media feeds that have an unknown safe search result.
  MediaHistoryKeyedService::PendingSafeSearchCheckList
  GetPendingSafeSearchCheckItems();

  // Stores the safe search result for |feed_id| and returns true if successful.
  bool StoreSafeSearchResult(int64_t feed_id,
                             media_feeds::mojom::SafeSearchResult result);

  // Updates the user status and returns true if successful.
  bool UpdateFeedUserStatus(const int64_t feed_id,
                            media_feeds::mojom::FeedUserStatus status);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_FEEDS_TABLE_H_
