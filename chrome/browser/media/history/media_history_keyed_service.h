// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_store.mojom.h"
#include "chrome/browser/media/kaleidoscope/mojom/kaleidoscope.mojom.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/media_player_watch_time.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/media_session/public/cpp/media_metadata.h"

class Profile;

namespace media_feeds {
class MediaFeedsService;
}  // namespace media_feeds

namespace media_session {
struct MediaImage;
struct MediaMetadata;
struct MediaPosition;
}  // namespace media_session

namespace history {
class HistoryService;
}  // namespace history

namespace media_history {

class MediaHistoryKeyedService : public KeyedService,
                                 public history::HistoryServiceObserver {
 public:
  explicit MediaHistoryKeyedService(Profile* profile);
  ~MediaHistoryKeyedService() override;

  static bool IsEnabled();

  // Returns the instance attached to the given |profile|.
  static MediaHistoryKeyedService* Get(Profile* profile);

  // Overridden from KeyedService:
  void Shutdown() override;

  // Overridden from history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // Saves a playback from a single player in the media history store.
  void SavePlayback(const content::MediaPlayerWatchTime& watch_time);

  void GetMediaHistoryStats(
      base::OnceCallback<void(mojom::MediaHistoryStatsPtr)> callback);

  // Returns all the rows in the origin table. This should only be used for
  // debugging because it is very slow.
  void GetOriginRowsForDebug(
      base::OnceCallback<void(std::vector<mojom::MediaHistoryOriginRowPtr>)>
          callback);

  // Returns all the rows in the playback table. This is only used for
  // debugging because it loads all rows in the table.
  void GetMediaHistoryPlaybackRowsForDebug(
      base::OnceCallback<void(std::vector<mojom::MediaHistoryPlaybackRowPtr>)>
          callback);

  // Gets the playback sessions from the media history store. The results will
  // be ordered by most recent first and be limited to the first |num_sessions|.
  // For each session it calls |filter| and if that returns |true| then that
  // session will be included in the results.
  using GetPlaybackSessionsFilter =
      base::RepeatingCallback<bool(const base::TimeDelta& duration,
                                   const base::TimeDelta& position)>;
  void GetPlaybackSessions(
      base::Optional<unsigned int> num_sessions,
      base::Optional<GetPlaybackSessionsFilter> filter,
      base::OnceCallback<void(
          std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>)> callback);

  // Saves a playback session in the media history store.
  void SavePlaybackSession(
      const GURL& url,
      const media_session::MediaMetadata& metadata,
      const base::Optional<media_session::MediaPosition>& position,
      const std::vector<media_session::MediaImage>& artwork);

  // Get origins from the origins table that have watchtime above the given
  // threshold value.
  void GetHighWatchTimeOrigins(
      const base::TimeDelta& audio_video_watchtime_min,
      base::OnceCallback<void(const std::vector<url::Origin>&)> callback);

  // Returns Media Feeds items.
  struct GetMediaFeedItemsRequest {
    enum class Type {
      // Return all the feed items for a feed for debugging.
      kDebugAll,

      // Returns items across all feeds that either have an active action status
      // or a play next candidate. Ordered by most recent first.
      kContinueWatching,

      // Returns all the items for a single feed. Ordered by clicked and shown
      // count so items that have been clicked and shown a lot will be at the
      // end. Items must not be continue watching items.
      kItemsForFeed
    };

    static GetMediaFeedItemsRequest CreateItemsForDebug(int64_t feed_id);

    static GetMediaFeedItemsRequest CreateItemsForFeed(
        int64_t feed_id,
        unsigned limit,
        bool fetched_items_should_be_safe,
        base::Optional<media_feeds::mojom::MediaFeedItemType> filter_by_type);

    static GetMediaFeedItemsRequest CreateItemsForContinueWatching(
        unsigned limit,
        bool fetched_items_should_be_safe,
        base::Optional<media_feeds::mojom::MediaFeedItemType> filter_by_type);

    GetMediaFeedItemsRequest();
    GetMediaFeedItemsRequest(const GetMediaFeedItemsRequest& t);

    Type type = Type::kDebugAll;

    // The ID of the feed to retrieve items for. Only valid for |kDebugAll| and
    // |kItemsForFeed|.
    base::Optional<int64_t> feed_id;

    // The maximum number of feeds to return. Only valid for |kContinueWatching|
    // and |kItemsForFeed|.
    base::Optional<unsigned> limit;

    // True if the item should have passed Safe Search checks. Only valid for
    // |kContinueWatching| and |kItemsForFeed|.
    bool fetched_items_should_be_safe = false;

    // The item type to filter by.
    base::Optional<media_feeds::mojom::MediaFeedItemType> filter_by_type;
  };
  void GetMediaFeedItems(
      const GetMediaFeedItemsRequest& request,
      base::OnceCallback<
          void(std::vector<media_feeds::mojom::MediaFeedItemPtr>)> callback);

  // Information about a completed media feed fetch, such as the feed items,
  // feed info, and fetch status code.
  struct MediaFeedFetchResult {
    ~MediaFeedFetchResult();
    MediaFeedFetchResult();
    MediaFeedFetchResult(MediaFeedFetchResult&& t);

    MediaFeedFetchResult(const MediaFeedFetchResult&) = delete;
    void operator=(const MediaFeedFetchResult&) = delete;

    int64_t feed_id;

    // The feed items that were fetched. Only contains valid items.
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items;

    // The status code for the fetch.
    media_feeds::mojom::FetchResult status;

    // If the feed was fetched from the browser cache then this should be true.
    bool was_fetched_from_cache = false;

    // Logos representing the feed.
    std::vector<media_feeds::mojom::MediaImagePtr> logos;

    // The display name for the feed.
    std::string display_name;

    // The reset token for the feed.
    base::Optional<base::UnguessableToken> reset_token;

    // Information about the currently logged in user.
    media_feeds::mojom::UserIdentifierPtr user_identifier;

    // If set then changes to the cookie name provided on the feed origin or any
    // associated origin will trigger the feed to be reset.
    std::string cookie_name_filter;

    // Logs about any errors that may have occurred while fetching or converting
    // the feed data. New-line delimited human-readable text.
    std::string error_logs;

    // If true then the backend returned a 410 Gone error.
    bool gone = false;
  };
  // Replaces the media items in |result.feed_id|. This will delete any old feed
  // items and store the new ones in |result.items|. This will also update the
  // |result.status|, |result.logos| and |result.display_name| for the feed.
  void StoreMediaFeedFetchResult(MediaFeedFetchResult result,
                                 base::OnceClosure callback);

  void GetURLsInTableForTest(const std::string& table,
                             base::OnceCallback<void(std::set<GURL>)> callback);

  // Represents an object that needs to be checked against Safe Search.
  // Contains the ID of the item and a set of URLs that should be checked.
  enum class SafeSearchCheckedType {
    kFeed,
    kFeedItem,
  };
  using SafeSearchID = std::pair<SafeSearchCheckedType, int64_t>;
  struct PendingSafeSearchCheck {
    PendingSafeSearchCheck(SafeSearchCheckedType type, int64_t id);
    ~PendingSafeSearchCheck();
    PendingSafeSearchCheck(const PendingSafeSearchCheck&) = delete;
    PendingSafeSearchCheck& operator=(const PendingSafeSearchCheck&) = delete;

    SafeSearchID const id;
    std::set<GURL> urls;
  };
  using PendingSafeSearchCheckList =
      std::vector<std::unique_ptr<PendingSafeSearchCheck>>;
  void GetPendingSafeSearchCheckMediaFeedItems(
      base::OnceCallback<void(PendingSafeSearchCheckList)> callback);

  // Store the Safe Search check results for multiple object. The map key is
  // the ID of the object.
  void StoreMediaFeedItemSafeSearchResults(
      std::map<SafeSearchID, media_feeds::mojom::SafeSearchResult> results);

  // Posts an empty task to the database thread. The callback will be called
  // on the calling thread when the empty task is completed. This can be used
  // for waiting for database operations in tests.
  void PostTaskToDBForTest(base::OnceClosure callback);

  // Returns Media Feeds.
  struct GetMediaFeedsRequest {
    enum class Type {
      // Return all the fields.
      kAll,

      // Returns the top feeds to be fetched. These will be sorted by the
      // by audio+video watchtime descending and we will also populate the
      // |origin_audio_video_watchtime_percentile| field in |MediaFeedPtr|.
      kTopFeedsForFetch,

      // Returns the top feeds to be displayed. These will be sorted by the
      // by audio+video watchtime descending and we will also populate the
      // |origin_audio_video_watchtime_percentile| field in |MediaFeedPtr|.
      kTopFeedsForDisplay,

      // Returns the feeeds that have been selected by the user to be fetched.
      kSelectedFeedsForFetch,
    };

    static GetMediaFeedsRequest CreateTopFeedsForFetch(
        unsigned limit,
        base::TimeDelta audio_video_watchtime_min);

    static GetMediaFeedsRequest CreateTopFeedsForDisplay(
        unsigned limit,
        int fetched_items_min,
        bool fetched_items_min_should_be_safe,
        base::Optional<media_feeds::mojom::MediaFeedItemType> filter_by_type);

    static GetMediaFeedsRequest CreateSelectedFeedsForFetch();

    GetMediaFeedsRequest();
    GetMediaFeedsRequest(const GetMediaFeedsRequest& t);

    Type type = Type::kAll;

    // The maximum number of feeds to return. Only valid for |kTopFeedsForFetch|
    // and |kTopFeedsForDisplay|.
    base::Optional<unsigned> limit;

    // The minimum audio+video watchtime required on the origin to return the
    // feed. Only valid for |kTopFeedsForFetch|.
    base::Optional<base::TimeDelta> audio_video_watchtime_min;

    // The minimum number of fetched items that are required and whether they
    // should have passed safe search. Only valid for |kTopFeedsForDisplay|.
    base::Optional<int> fetched_items_min;
    bool fetched_items_min_should_be_safe = false;

    // The item type to filter by.
    base::Optional<media_feeds::mojom::MediaFeedItemType> filter_by_type;
  };
  void GetMediaFeeds(
      const GetMediaFeedsRequest& request,
      base::OnceCallback<void(std::vector<media_feeds::mojom::MediaFeedPtr>)>
          callback);

  // Updates the display time for the Media Feed with |feed_id| to now.
  void UpdateMediaFeedDisplayTime(const int64_t feed_id);

  // Increment the media feed items shown counter by one.
  void IncrementMediaFeedItemsShownCount(const std::set<int64_t> feed_item_ids);

  // Marks a media feed item as clicked. This is when the user has opened the
  // item in the UI.
  void MarkMediaFeedItemAsClicked(const int64_t& feed_item_id);

  // Resets a Media Feed by deleting any items and resetting it to defaults. If
  // |include_subdomains| is true then this will reset any feeds on any
  // subdomain of |origin|.
  void ResetMediaFeedDueToCookies(const url::Origin& origin,
                                  const bool include_subdomains,
                                  const std::string& name,
                                  const net::CookieChangeCause& cause);

  // Resets any Media Feeds that were fetched between |start_time| and
  // |end_time|. This will delete any items and reset them to defaults. The
  // reason will be set to |kCache|.
  using CacheClearingFilter = base::RepeatingCallback<bool(const GURL& url)>;
  void ResetMediaFeedDueToCacheClearing(const base::Time& start_time,
                                        const base::Time& end_time,
                                        CacheClearingFilter filter,
                                        base::OnceClosure callback);

  // Deletes the Media Feed and runs the callback.
  void DeleteMediaFeed(const int64_t feed_id, base::OnceClosure callback);

  // Gets the details needed to fetch a Media Feed.
  struct MediaFeedFetchDetails {
    MediaFeedFetchDetails();
    ~MediaFeedFetchDetails();
    MediaFeedFetchDetails(MediaFeedFetchDetails&& t);
    MediaFeedFetchDetails& operator=(const MediaFeedFetchDetails&);

    GURL url;
    media_feeds::mojom::FetchResult last_fetch_result;
    base::Optional<base::UnguessableToken> reset_token;
  };
  using GetMediaFeedFetchDetailsCallback =
      base::OnceCallback<void(base::Optional<MediaFeedFetchDetails>)>;
  void GetMediaFeedFetchDetails(const int64_t feed_id,
                                GetMediaFeedFetchDetailsCallback callback);

  // Updates the FeedUserStatus for a feed.
  void UpdateFeedUserStatus(const int64_t feed_id,
                            media_feeds::mojom::FeedUserStatus status);

  // Stores the Kaleidocope data keyed against a GAIA ID.
  void SetKaleidoscopeData(media::mojom::GetCollectionsResponsePtr data,
                           const std::string& gaia_id);

  // Retrieves the Kaleidoscope data keyed against a GAIA ID. The data expires
  // after 24 hours or if the GAIA ID changes.
  using GetKaleidoscopeDataCallback =
      base::OnceCallback<void(media::mojom::GetCollectionsResponsePtr)>;
  void GetKaleidoscopeData(const std::string& gaia_id,
                           GetKaleidoscopeDataCallback callback);

  // Delete any stored data.
  void DeleteKaleidoscopeData();

 protected:
  friend class media_feeds::MediaFeedsService;

  // Resets a Media Feed by deleting any items and resetting it to defaults. If
  // |include_subdomains| is true then this will reset any feeds on any
  // subdomain of |origin|.
  void ResetMediaFeed(const url::Origin& origin,
                      media_feeds::mojom::ResetReason reason);

  // Saves a newly discovered media feed in the media history store.
  void DiscoverMediaFeed(const GURL& url,
                         base::OnceClosure callback = base::DoNothing());

 private:
  class StoreHolder;

  std::unique_ptr<StoreHolder> store_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(MediaHistoryKeyedService);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_
