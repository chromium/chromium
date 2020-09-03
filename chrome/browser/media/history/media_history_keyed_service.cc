// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_keyed_service.h"

#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "media/base/media_switches.h"

namespace media_history {

// StoreHolder will in most cases hold a local MediaHistoryStore. However, for
// OTR profiles we hold a pointer to the original profile store. When accessing
// MediaHistoryStore you should use GetForRead for read operations,
// GetForWrite for write operations and GetForDelete for delete operations.
// These can be null if the store is read only or we disable storing browsing
// history.
class MediaHistoryKeyedService::StoreHolder {
 public:
  StoreHolder(Profile* profile,
              scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner,
              const bool should_reset)
      : profile_(profile),
        local_(new MediaHistoryStore(profile, db_task_runner)),
        db_task_runner_(db_task_runner) {
    local_->db_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaHistoryStore::Initialize,
                                  base::RetainedRef(local_), should_reset));
  }

  explicit StoreHolder(Profile* profile, MediaHistoryKeyedService* remote)
      : profile_(profile), remote_(remote) {}

  ~StoreHolder() = default;
  StoreHolder(const StoreHolder& t) = delete;
  StoreHolder& operator=(const StoreHolder&) = delete;

  MediaHistoryStore* GetForRead() {
    if (local_)
      return local_.get();
    return remote_->store_->GetForRead();
  }

  MediaHistoryStore* GetForWrite() {
    if (profile_->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled))
      return nullptr;
    if (local_)
      return local_.get();
    return nullptr;
  }

  MediaHistoryStore* GetForDelete() {
    if (local_)
      return local_.get();
    return nullptr;
  }

  void Shutdown() {
    if (!local_)
      return;

    local_->SetCancelled();
  }

 private:
  Profile* profile_;
  scoped_refptr<MediaHistoryStore> local_;
  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;
  MediaHistoryKeyedService* remote_ = nullptr;
};

MediaHistoryKeyedService::MediaHistoryKeyedService(Profile* profile)
    : profile_(profile) {
  // May be null in tests.
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  if (history)
    history->AddObserver(this);

  if (profile->IsOffTheRecord()) {
    MediaHistoryKeyedService* original =
        MediaHistoryKeyedService::Get(profile->GetOriginalProfile());
    DCHECK(original);

    store_ = std::make_unique<StoreHolder>(profile, original);
  } else {
    auto db_task_runner = base::ThreadPool::CreateUpdateableSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

    store_ = std::make_unique<StoreHolder>(profile_, std::move(db_task_runner),
                                           /* should_reset=*/false);
  }
}

// static
MediaHistoryKeyedService* MediaHistoryKeyedService::Get(Profile* profile) {
  return MediaHistoryKeyedServiceFactory::GetForProfile(profile);
}

MediaHistoryKeyedService::~MediaHistoryKeyedService() = default;

bool MediaHistoryKeyedService::IsEnabled() {
  return base::FeatureList::IsEnabled(media::kUseMediaHistoryStore);
}

void MediaHistoryKeyedService::Shutdown() {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);
  if (history)
    history->RemoveObserver(this);

  store_->Shutdown();
}

void MediaHistoryKeyedService::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // The store might not always be writable.
  auto* store = store_->GetForDelete();
  if (!store)
    return;

  if (deletion_info.IsAllHistory()) {
    // Stop the old database and destroy the DB.
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner =
        store->db_task_runner_;
    store->SetCancelled();
    store_.reset();

    // Create a new internal store.
    store_ = std::make_unique<StoreHolder>(profile_, std::move(db_task_runner),
                                           /* should_reset= */ true);
    return;
  }

  // Build a set of all urls and origins in |deleted_rows|.
  std::set<url::Origin> origins;
  for (const history::URLRow& row : deletion_info.deleted_rows()) {
    origins.insert(url::Origin::Create(row.url()));
  }

  // Find any origins that do not have any more data in the history database.
  std::set<url::Origin> deleted_origins;
  for (const url::Origin& origin : origins) {
    const auto& origin_count =
        deletion_info.deleted_urls_origin_map().find(origin.GetURL());

    if (origin_count == deletion_info.deleted_urls_origin_map().end() ||
        origin_count->second.first > 0) {
      continue;
    }

    deleted_origins.insert(origin);
  }

  if (!deleted_origins.empty()) {
    store->db_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaHistoryStore::DeleteAllOriginData,
                                  store, deleted_origins));
  }

  // Build a set of all urls in |deleted_rows| that do not have their origin in
  // |deleted_origins|.
  std::set<GURL> deleted_urls;
  for (const history::URLRow& row : deletion_info.deleted_rows()) {
    auto origin = url::Origin::Create(row.url());

    if (base::Contains(deleted_origins, origin))
      continue;

    deleted_urls.insert(row.url());
  }

  if (!deleted_urls.empty()) {
    store->db_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaHistoryStore::DeleteAllURLData, store,
                                  deleted_urls));
  }
}

void MediaHistoryKeyedService::SavePlayback(
    const content::MediaPlayerWatchTime& watch_time) {
  if (auto* store = store_->GetForWrite()) {
    store->db_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::SavePlayback, store, watch_time));
  }
}

void MediaHistoryKeyedService::GetMediaHistoryStats(
    base::OnceCallback<void(mojom::MediaHistoryStatsPtr)> callback) {
  base::PostTaskAndReplyWithResult(
      store_->GetForRead()->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetMediaHistoryStats,
                     store_->GetForRead()),
      std::move(callback));
}

void MediaHistoryKeyedService::GetOriginRowsForDebug(
    base::OnceCallback<void(std::vector<mojom::MediaHistoryOriginRowPtr>)>
        callback) {
  base::PostTaskAndReplyWithResult(
      store_->GetForRead()->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetOriginRowsForDebug,
                     store_->GetForRead()),
      std::move(callback));
}

void MediaHistoryKeyedService::GetMediaHistoryPlaybackRowsForDebug(
    base::OnceCallback<void(std::vector<mojom::MediaHistoryPlaybackRowPtr>)>
        callback) {
  base::PostTaskAndReplyWithResult(
      store_->GetForRead()->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetMediaHistoryPlaybackRowsForDebug,
                     store_->GetForRead()),
      std::move(callback));
}

void MediaHistoryKeyedService::GetPlaybackSessions(
    base::Optional<unsigned int> num_sessions,
    base::Optional<GetPlaybackSessionsFilter> filter,
    base::OnceCallback<
        void(std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>)> callback) {
  base::PostTaskAndReplyWithResult(
      store_->GetForRead()->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetPlaybackSessions,
                     store_->GetForRead(), num_sessions, std::move(filter)),
      std::move(callback));
}

void MediaHistoryKeyedService::SavePlaybackSession(
    const GURL& url,
    const media_session::MediaMetadata& metadata,
    const base::Optional<media_session::MediaPosition>& position,
    const std::vector<media_session::MediaImage>& artwork) {
  if (auto* store = store_->GetForWrite()) {
    store->db_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaHistoryStore::SavePlaybackSession,
                                  store, url, metadata, position, artwork));
  }
}

void MediaHistoryKeyedService::GetHighWatchTimeOrigins(
    const base::TimeDelta& audio_video_watchtime_min,
    base::OnceCallback<void(const std::vector<url::Origin>&)> callback) {
  base::PostTaskAndReplyWithResult(
      store_->GetForRead()->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetHighWatchTimeOrigins,
                     store_->GetForRead(), audio_video_watchtime_min),
      std::move(callback));
}

MediaHistoryKeyedService::GetMediaFeedItemsRequest
MediaHistoryKeyedService::GetMediaFeedItemsRequest::CreateItemsForDebug(
    int64_t feed_id) {
  GetMediaFeedItemsRequest request;
  request.type = Type::kDebugAll;
  request.feed_id = feed_id;
  return request;
}

MediaHistoryKeyedService::GetMediaFeedItemsRequest
MediaHistoryKeyedService::GetMediaFeedItemsRequest::CreateItemsForFeed(
    int64_t feed_id,
    unsigned limit,
    bool fetched_items_should_be_safe,
    base::Optional<media_feeds::mojom::MediaFeedItemType> filter_by_type) {
  GetMediaFeedItemsRequest request;
  request.type = Type::kItemsForFeed;
  request.limit = limit;
  request.feed_id = feed_id;
  request.fetched_items_should_be_safe = fetched_items_should_be_safe;
  request.filter_by_type = filter_by_type;
  return request;
}

MediaHistoryKeyedService::GetMediaFeedItemsRequest MediaHistoryKeyedService::
    GetMediaFeedItemsRequest::CreateItemsForContinueWatching(
        unsigned limit,
        bool fetched_items_should_be_safe,
        base::Optional<media_feeds::mojom::MediaFeedItemType> filter_by_type) {
  GetMediaFeedItemsRequest request;
  request.type = Type::kContinueWatching;
  request.limit = limit;
  request.fetched_items_should_be_safe = fetched_items_should_be_safe;
  request.filter_by_type = filter_by_type;
  return request;
}

MediaHistoryKeyedService::GetMediaFeedItemsRequest::GetMediaFeedItemsRequest() =
    default;

MediaHistoryKeyedService::GetMediaFeedItemsRequest::GetMediaFeedItemsRequest(
    const GetMediaFeedItemsRequest& t) = default;

void MediaHistoryKeyedService::GetMediaFeedItems(
    const GetMediaFeedItemsRequest& request,
    base::OnceCallback<void(std::vector<media_feeds::mojom::MediaFeedItemPtr>)>
        callback) {
  base::PostTaskAndReplyWithResult(
      store_->GetForRead()->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetMediaFeedItems,
                     store_->GetForRead(), request),
      std::move(callback));
}

MediaHistoryKeyedService::MediaFeedFetchResult::MediaFeedFetchResult() =
    default;

MediaHistoryKeyedService::MediaFeedFetchResult::~MediaFeedFetchResult() =
    default;

MediaHistoryKeyedService::MediaFeedFetchResult::MediaFeedFetchResult(
    MediaFeedFetchResult&& t) = default;

void MediaHistoryKeyedService::StoreMediaFeedFetchResult(
    MediaFeedFetchResult result,
    base::OnceClosure callback) {
  if (auto* store = store_->GetForWrite()) {
    store->db_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::StoreMediaFeedFetchResult, store,
                       std::move(result)),
        std::move(callback));
  }
}

void MediaHistoryKeyedService::GetURLsInTableForTest(
    const std::string& table,
    base::OnceCallback<void(std::set<GURL>)> callback) {
  base::PostTaskAndReplyWithResult(
      store_->GetForRead()->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetURLsInTableForTest,
                     store_->GetForRead(), table),
      std::move(callback));
}

void MediaHistoryKeyedService::DiscoverMediaFeed(const GURL& url,
                                                 base::OnceClosure callback) {
  if (auto* store = store_->GetForWrite()) {
    store->db_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::DiscoverMediaFeed, store, url),
        std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

MediaHistoryKeyedService::PendingSafeSearchCheck::PendingSafeSearchCheck(
    SafeSearchCheckedType type,
    int64_t id)
    : id(std::make_pair(type, id)) {}

MediaHistoryKeyedService::PendingSafeSearchCheck::~PendingSafeSearchCheck() =
    default;

void MediaHistoryKeyedService::GetPendingSafeSearchCheckMediaFeedItems(
    base::OnceCallback<void(PendingSafeSearchCheckList)> callback) {
  base::PostTaskAndReplyWithResult(
      store_->GetForRead()->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &MediaHistoryStore::GetPendingSafeSearchCheckMediaFeedItems,
          store_->GetForRead()),
      std::move(callback));
}

void MediaHistoryKeyedService::StoreMediaFeedItemSafeSearchResults(
    std::map<SafeSearchID, media_feeds::mojom::SafeSearchResult> results) {
  if (auto* store = store_->GetForWrite()) {
    store->db_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::StoreMediaFeedItemSafeSearchResults,
                       store, results));
  }
}

void MediaHistoryKeyedService::PostTaskToDBForTest(base::OnceClosure callback) {
  store_->GetForRead()->db_task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), std::move(callback));
}

MediaHistoryKeyedService::GetMediaFeedsRequest
MediaHistoryKeyedService::GetMediaFeedsRequest::CreateTopFeedsForFetch(
    unsigned limit,
    base::TimeDelta audio_video_watchtime_min) {
  GetMediaFeedsRequest request;
  request.type = Type::kTopFeedsForFetch;
  request.limit = limit;
  request.audio_video_watchtime_min = audio_video_watchtime_min;
  return request;
}

MediaHistoryKeyedService::GetMediaFeedsRequest
MediaHistoryKeyedService::GetMediaFeedsRequest::CreateTopFeedsForDisplay(
    unsigned limit,
    int fetched_items_min,
    bool fetched_items_min_should_be_safe,
    base::Optional<media_feeds::mojom::MediaFeedItemType> filter_by_type) {
  GetMediaFeedsRequest request;
  request.type = Type::kTopFeedsForDisplay;
  request.limit = limit;
  request.fetched_items_min = fetched_items_min;
  request.fetched_items_min_should_be_safe = fetched_items_min_should_be_safe;
  request.filter_by_type = filter_by_type;
  return request;
}

MediaHistoryKeyedService::GetMediaFeedsRequest
MediaHistoryKeyedService::GetMediaFeedsRequest::CreateSelectedFeedsForFetch() {
  GetMediaFeedsRequest request;
  request.type = Type::kSelectedFeedsForFetch;
  return request;
}

MediaHistoryKeyedService::GetMediaFeedsRequest::GetMediaFeedsRequest() =
    default;

MediaHistoryKeyedService::GetMediaFeedsRequest::GetMediaFeedsRequest(
    const GetMediaFeedsRequest& t) = default;

void MediaHistoryKeyedService::GetMediaFeeds(
    const GetMediaFeedsRequest& request,
    base::OnceCallback<void(std::vector<media_feeds::mojom::MediaFeedPtr>)>
        callback) {
  base::PostTaskAndReplyWithResult(
      store_->GetForRead()->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetMediaFeeds, store_->GetForRead(),
                     request),
      std::move(callback));
}

void MediaHistoryKeyedService::UpdateMediaFeedDisplayTime(
    const int64_t feed_id) {
  if (auto* store = store_->GetForWrite()) {
    store->db_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::UpdateMediaFeedDisplayTime, store,
                       feed_id));
  }
}

void MediaHistoryKeyedService::IncrementMediaFeedItemsShownCount(
    const std::set<int64_t> feed_item_ids) {
  if (auto* store = store_->GetForWrite()) {
    store->db_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::IncrementMediaFeedItemsShownCount,
                       base::RetainedRef(store), feed_item_ids));
  }
}

void MediaHistoryKeyedService::MarkMediaFeedItemAsClicked(
    const int64_t& feed_item_id) {
  if (auto* store = store_->GetForWrite()) {
    store->db_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::MarkMediaFeedItemAsClicked,
                       base::RetainedRef(store), feed_item_id));
  }
}

void MediaHistoryKeyedService::ResetMediaFeed(
    const url::Origin& origin,
    media_feeds::mojom::ResetReason reason) {
  CHECK_NE(media_feeds::mojom::ResetReason::kNone, reason);

  if (auto* store = store_->GetForDelete()) {
    store->db_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaHistoryStore::ResetMediaFeed, store,
                                  origin, reason));
  }
}

void MediaHistoryKeyedService::ResetMediaFeedDueToCookies(
    const url::Origin& origin,
    const bool include_subdomains,
    const std::string& name,
    const net::CookieChangeCause& cause) {
  if (auto* store = store_->GetForDelete()) {
    store->db_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::ResetMediaFeedDueToCookies, store,
                       origin, include_subdomains, name, cause));
  }
}

void MediaHistoryKeyedService::ResetMediaFeedDueToCacheClearing(
    const base::Time& start_time,
    const base::Time& end_time,
    CacheClearingFilter filter,
    base::OnceClosure callback) {
  if (auto* store = store_->GetForDelete()) {
    store->db_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::ResetMediaFeedDueToCacheClearing,
                       store, start_time, end_time, std::move(filter)),
        std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void MediaHistoryKeyedService::DeleteMediaFeed(const int64_t feed_id,
                                               base::OnceClosure callback) {
  if (auto* store = store_->GetForDelete()) {
    store->db_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::DeleteMediaFeed, store, feed_id),
        std::move(callback));
  }
}

void MediaHistoryKeyedService::UpdateFeedUserStatus(
    const int64_t feed_id,
    media_feeds::mojom::FeedUserStatus status) {
  if (auto* store = store_->GetForWrite()) {
    store->db_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaHistoryStore::UpdateFeedUserStatus,
                                  store, feed_id, status));
  }
}

MediaHistoryKeyedService::MediaFeedFetchDetails::MediaFeedFetchDetails() =
    default;

MediaHistoryKeyedService::MediaFeedFetchDetails::~MediaFeedFetchDetails() =
    default;

MediaHistoryKeyedService::MediaFeedFetchDetails::MediaFeedFetchDetails(
    MediaFeedFetchDetails&& t) = default;

MediaHistoryKeyedService::MediaFeedFetchDetails&
MediaHistoryKeyedService::MediaFeedFetchDetails::operator=(
    const MediaHistoryKeyedService::MediaFeedFetchDetails&) = default;

void MediaHistoryKeyedService::GetMediaFeedFetchDetails(
    const int64_t feed_id,
    GetMediaFeedFetchDetailsCallback callback) {
  base::PostTaskAndReplyWithResult(
      store_->GetForRead()->db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetMediaFeedFetchDetails,
                     store_->GetForRead(), feed_id),
      std::move(callback));
}

void MediaHistoryKeyedService::SetKaleidoscopeData(
    media::mojom::GetCollectionsResponsePtr data,
    const std::string& gaia_id) {
  if (auto* store = store_->GetForWrite()) {
    store->db_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaHistoryStore::SetKaleidoscopeData,
                                  store, std::move(data), gaia_id));
  }
}

void MediaHistoryKeyedService::GetKaleidoscopeData(
    const std::string& gaia_id,
    GetKaleidoscopeDataCallback callback) {
  if (auto* store = store_->GetForWrite()) {
    base::PostTaskAndReplyWithResult(
        store_->GetForRead()->db_task_runner_.get(), FROM_HERE,
        base::BindOnce(&MediaHistoryStore::GetKaleidoscopeData, store, gaia_id),
        std::move(callback));
  } else {
    std::move(callback).Run(nullptr);
  }
}

void MediaHistoryKeyedService::DeleteKaleidoscopeData() {
  if (auto* store = store_->GetForWrite()) {
    store->db_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::DeleteKaleidoscopeData, store));
  }
}

}  // namespace media_history
