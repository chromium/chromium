// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_retry_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_api_error.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_delegate.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_controller.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "google_apis/common/api_error_codes.h"
#include "url/gurl.h"

namespace ash {

inline constexpr size_t kYouTubeMusicPlaylistBucketCount = 3;

// This class handles requests from `FocusModeSoundsDelegate` interface. It
// talks to YouTube Music API backend asynchronously, and returns results via
// given callbacks. It handles one request of a kind at a time, which means
// consecutive requests of the same kind would overwrite the previous one. It
// also invokes callbacks strictly, i.e. when successful, failed, or
// overwritten, it would run the given callbacks with valid/empty data.
class ASH_EXPORT FocusModeYouTubeMusicDelegate
    : public FocusModeSoundsDelegate {
 public:
  FocusModeYouTubeMusicDelegate();
  ~FocusModeYouTubeMusicDelegate() override;

  youtube_music::YouTubeMusicController* youtube_music_controller() const {
    return youtube_music_controller_.get();
  }

  // FocusModeSoundsDelegate:
  void GetNextTrack(const std::string& playlist_id,
                    FocusModeSoundsDelegate::TrackCallback callback) override;
  void GetPlaylists(
      FocusModeSoundsDelegate::PlaylistsCallback callback) override;

  void SetNoPremiumCallback(base::RepeatingClosure callback);

  // `callback` is run whenever the API encounters an error resulting in a
  // request failure.
  void SetErrorCallback(ApiErrorCallback callback);

  // Returns the most recent error received from the API. If an error is fatal,
  // requests will stop so the same error will be returned. For non-fatal
  // errors, the error will be cleared by a successful request.
  const std::optional<FocusModeApiError>& last_api_error() const {
    return last_error_;
  }

  // Reports music playback.
  void ReportPlayback(const youtube_music::PlaybackData& playback_data);

  // Reserves a playlist for the returned playlists.
  void ReservePlaylistForGetPlaylists(const std::string& playlist_id);

 private:
  // Struct that keeps track of ongoing `GetPlaylists` request. It contains
  // enough information about how the current request should be done.
  struct GetPlaylistsRequestState {
    enum class PlaylistType {
      kFocusSuperMix,
      kReserved,
      kFocusIntent,
    };

    GetPlaylistsRequestState();
    ~GetPlaylistsRequestState();

    void Reset();

    void ResetDoneCallback();

    std::vector<FocusModeSoundsDelegate::Playlist> GetTopPlaylists();

    // Data structure that holds data from multiple API requests. It's organized
    // in buckets so that the returned list is ordered.
    std::array<std::vector<Playlist>, kYouTubeMusicPlaylistBucketCount>
        playlist_buckets;

    // Reserved playlist to query if set.
    std::optional<std::string> reserved_playlist_id;

    // Target number of API requests.
    int target_count = 0;

    // Count of current done API requests.
    int count = 0;

    // Callback to run when this request is successful, failed, or overwritten.
    FocusModeSoundsDelegate::PlaylistsCallback done_callback;

    std::array<FocusModeRetryState, kYouTubeMusicPlaylistBucketCount>
        retry_states;
  };

  // Struct that keeps track of ongoing `GetNextTrack` request. It contains
  // enough information about how the current request should be done.
  struct GetNextTrackRequestState {
    GetNextTrackRequestState();
    ~GetNextTrackRequestState();

    void Reset();

    void ResetDoneCallback();

    std::string last_playlist_id;
    std::string last_queue_id;
    FocusModeSoundsDelegate::TrackCallback done_callback;
    FocusModeRetryState retry_state;
  };

  // Struct that keeps track of ongoing `ReportPlaybackRequest` request. It
  // contains enough information about how the current request should be done.
  struct ReportPlaybackRequestState {
    ReportPlaybackRequestState();
    ~ReportPlaybackRequestState();

    youtube_music::PlaybackState playback_state;
    std::optional<youtube_music::PlaybackData> staged_playback_data;
    std::string token;
    FocusModeRetryState retry_state;
  };

  // Triggers request to query for specific playlist for the given bucket.
  void GetPlaylistInternal(const GetPlaylistsRequestState::PlaylistType type);

  // Called when get playlists request is done.
  void OnGetPlaylistDone(
      const base::Time start_time,
      const GetPlaylistsRequestState::PlaylistType type,
      base::expected<youtube_music::Playlist,
                     google_apis::youtube_music::ApiError> playlist);

  // Triggers request to query for focus-intent playlists for the given bucket.
  void GetMusicSectionInternal();

  // Called when get music section request is done.
  void OnGetMusicSectionDone(
      const base::Time start_time,
      base::expected<const std::vector<youtube_music::Playlist>,
                     google_apis::youtube_music::ApiError> playlists);

  // Invoked when sub-request is done for `GetPlaylists()`. It's responsible for
  // collecting the data, and reporting the data back when we are at the target
  // count.
  void MaybeReportBackPlaylists();

  // Triggers request to get next track depending on the current request state.
  void GetNextTrackInternal(const std::string& playlist_id);

  // Called when switching to next track is done.
  void OnNextTrackDone(
      const base::Time start_time,
      const bool prepare,
      const std::string& playlist_id,
      base::expected<const youtube_music::PlaybackContext,
                     google_apis::youtube_music::ApiError> playback_context);

  void ReportPlaybackInternal(const GURL& url);

  // Called when report playback request is done.
  void OnReportPlaybackDone(
      const base::Time start_time,
      const GURL& url,
      base::expected<const std::string, google_apis::youtube_music::ApiError>
          new_playback_reporting_token);

  bool ContainsFatalError() const;
  void RequestSuccessful();
  void ApiErrorEncountered(FocusModeApiError error);

  std::optional<FocusModeApiError> last_error_;

  // Playlists request state for `GetPlaylists`.
  GetPlaylistsRequestState get_playlists_state_;

  // Next track request state for `GetPlaylists`.
  GetNextTrackRequestState next_track_state_;

  // Report playback request state per track for `ReportPlayback`.
  base::flat_map<GURL, std::unique_ptr<ReportPlaybackRequestState>>
      report_playback_states_;

  // Callback to run when the request fails with HTTP 403.
  base::RepeatingClosure no_premium_callback_;

  ApiErrorCallback error_callback_ = base::NullCallback();

  // Controller for YouTube Music API integration.
  std::unique_ptr<youtube_music::YouTubeMusicController>
      youtube_music_controller_;

  base::WeakPtrFactory<FocusModeYouTubeMusicDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_YOUTUBE_MUSIC_DELEGATE_H_
