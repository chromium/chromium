// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_keyed_service.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
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
        FROM_HERE,
        base::BindOnce(&MediaHistoryStore::Initialize, local_, should_reset));
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
  raw_ptr<Profile, DanglingUntriaged> profile_;
  scoped_refptr<MediaHistoryStore> local_;
  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;
  raw_ptr<MediaHistoryKeyedService, DanglingUntriaged> remote_ = nullptr;
};

MediaHistoryKeyedService::MediaHistoryKeyedService(Profile* profile)
    : profile_(profile) {
  // May be null in tests.
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  if (history)
    history_service_observation_.Observe(history);

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
  history_service_observation_.Reset();
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
        base::BindOnce(
            &MediaHistoryStore::SavePlayback, store,
            std::make_unique<content::MediaPlayerWatchTime>(
                watch_time.url, watch_time.origin,
                watch_time.cumulative_watch_time, watch_time.last_timestamp,
                watch_time.has_video, watch_time.has_audio)));
  }
}

void MediaHistoryKeyedService::GetMediaHistoryStats(
    base::OnceCallback<void(mojom::MediaHistoryStatsPtr)> callback) {
  store_->GetForRead()->db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetMediaHistoryStats,
                     store_->GetForRead()),
      std::move(callback));
}

void MediaHistoryKeyedService::GetOriginRowsForDebug(
    base::OnceCallback<void(std::vector<mojom::MediaHistoryOriginRowPtr>)>
        callback) {
  store_->GetForRead()->db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetOriginRowsForDebug,
                     store_->GetForRead()),
      std::move(callback));
}

void MediaHistoryKeyedService::GetMediaHistoryPlaybackRowsForDebug(
    base::OnceCallback<void(std::vector<mojom::MediaHistoryPlaybackRowPtr>)>
        callback) {
  store_->GetForRead()->db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetMediaHistoryPlaybackRowsForDebug,
                     store_->GetForRead()),
      std::move(callback));
}

void MediaHistoryKeyedService::GetPlaybackSessions(
    absl::optional<unsigned int> num_sessions,
    absl::optional<GetPlaybackSessionsFilter> filter,
    base::OnceCallback<
        void(std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>)> callback) {
  store_->GetForRead()->db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetPlaybackSessions,
                     store_->GetForRead(), num_sessions, std::move(filter)),
      std::move(callback));
}

void MediaHistoryKeyedService::SavePlaybackSession(
    const GURL& url,
    const media_session::MediaMetadata& metadata,
    const absl::optional<media_session::MediaPosition>& position,
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
  store_->GetForRead()->db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetHighWatchTimeOrigins,
                     store_->GetForRead(), audio_video_watchtime_min),
      std::move(callback));
}

void MediaHistoryKeyedService::GetURLsInTableForTest(
    const std::string& table,
    base::OnceCallback<void(std::set<GURL>)> callback) {
  store_->GetForRead()->db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaHistoryStore::GetURLsInTableForTest,
                     store_->GetForRead(), table),
      std::move(callback));
}

void MediaHistoryKeyedService::PostTaskToDBForTest(base::OnceClosure callback) {
  store_->GetForRead()->db_task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), std::move(callback));
}

}  // namespace media_history
