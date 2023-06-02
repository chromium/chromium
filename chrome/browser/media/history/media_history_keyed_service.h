// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/media/history/media_history_store.mojom.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/media_player_watch_time.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/media_session/public/cpp/media_metadata.h"

class Profile;

namespace media_session {
struct MediaImage;
struct MediaMetadata;
struct MediaPosition;
}  // namespace media_session

namespace media_history {

class MediaHistoryKeyedService : public KeyedService,
                                 public history::HistoryServiceObserver {
 public:
  explicit MediaHistoryKeyedService(Profile* profile);

  MediaHistoryKeyedService(const MediaHistoryKeyedService&) = delete;
  MediaHistoryKeyedService& operator=(const MediaHistoryKeyedService&) = delete;

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
      absl::optional<unsigned int> num_sessions,
      absl::optional<GetPlaybackSessionsFilter> filter,
      base::OnceCallback<void(
          std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>)> callback);

  // Saves a playback session in the media history store.
  void SavePlaybackSession(
      const GURL& url,
      const media_session::MediaMetadata& metadata,
      const absl::optional<media_session::MediaPosition>& position,
      const std::vector<media_session::MediaImage>& artwork);

  // Get origins from the origins table that have watchtime above the given
  // threshold value.
  void GetHighWatchTimeOrigins(
      const base::TimeDelta& audio_video_watchtime_min,
      base::OnceCallback<void(const std::vector<url::Origin>&)> callback);

  void GetURLsInTableForTest(const std::string& table,
                             base::OnceCallback<void(std::set<GURL>)> callback);

  // Posts an empty task to the database thread. The callback will be called
  // on the calling thread when the empty task is completed. This can be used
  // for waiting for database operations in tests.
  void PostTaskToDBForTest(base::OnceClosure callback);

 private:
  class StoreHolder;

  std::unique_ptr<StoreHolder> store_;

  raw_ptr<Profile, DanglingUntriaged> profile_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_
