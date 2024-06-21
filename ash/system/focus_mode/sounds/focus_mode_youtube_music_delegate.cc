// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_youtube_music_delegate.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "google_apis/common/api_error_codes.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr size_t kPlaylistNum = 4;
constexpr char kFocusSupermixPlaylistId[] =
    "playlists/RDTMAK5uy_l3TXw3uC_sIHl4m6RMGqCyKKd2D2_pv28";
constexpr char kYouTubeMusicSourceFormat[] = "YouTube Music ᐧ %s";

youtube_music::YouTubeMusicController* GetYouTubeMusicController() {
  if (auto* focus_mode_controller = FocusModeController::Get()) {
    return focus_mode_controller->youtube_music_controller();
  }
  return nullptr;
}
}  // namespace

FocusModeYouTubeMusicDelegate::FocusModeYouTubeMusicDelegate() = default;
FocusModeYouTubeMusicDelegate::~FocusModeYouTubeMusicDelegate() = default;

bool FocusModeYouTubeMusicDelegate::GetNextTrack(
    const std::string& playlist_id,
    FocusModeSoundsDelegate::TrackCallback callback) {
  CHECK(callback);
  next_track_state_.ResetDoneCallback();

  auto* youtube_music_controller = GetYouTubeMusicController();
  if (!youtube_music_controller) {
    std::move(callback).Run(std::nullopt);
    return false;
  }

  if (next_track_state_.last_playlist_id != playlist_id) {
    next_track_state_.done_callback = std::move(callback);
    youtube_music_controller->PlaybackQueuePrepare(
        playlist_id,
        base::BindOnce(&FocusModeYouTubeMusicDelegate::OnNextTrackDone,
                       weak_factory_.GetWeakPtr(), playlist_id));
  } else {
    next_track_state_.done_callback = std::move(callback);
    youtube_music_controller->PlaybackQueueNext(
        next_track_state_.last_queue_id,
        base::BindOnce(&FocusModeYouTubeMusicDelegate::OnNextTrackDone,
                       weak_factory_.GetWeakPtr(), playlist_id));
  }

  return true;
}

bool FocusModeYouTubeMusicDelegate::GetPlaylists(
    FocusModeSoundsDelegate::PlaylistsCallback callback) {
  CHECK(callback);
  get_playlists_state_.Reset();

  auto* youtube_music_controller = GetYouTubeMusicController();
  if (!youtube_music_controller) {
    std::move(callback).Run({});
    return false;
  }

  // Cache the done callback, add focus supermix/reserved playlist to the to-do
  // list, and update the total number of API request to run.
  get_playlists_state_.done_callback = std::move(callback);
  if (get_playlists_state_.reserved_playlist_id) {
    get_playlists_state_
        .playlists_to_query[get_playlists_state_.reserved_playlist_id.value()] =
        1;
  }
  get_playlists_state_.playlists_to_query[kFocusSupermixPlaylistId] = 0;
  get_playlists_state_.target_count =
      get_playlists_state_.playlists_to_query.size() + 1;

  // Invoke the API requests.
  for (const auto& [playlist_id, playlist_bucket] :
       get_playlists_state_.playlists_to_query) {
    youtube_music_controller->GetPlaylist(
        playlist_id,
        base::BindOnce(&FocusModeYouTubeMusicDelegate::OnGetPlaylistDone,
                       weak_factory_.GetWeakPtr(), playlist_bucket));
  }
  youtube_music_controller->GetMusicSection(
      base::BindOnce(&FocusModeYouTubeMusicDelegate::OnGetMusicSectionDone,
                     weak_factory_.GetWeakPtr(), /*bucket=*/2));

  return true;
}

void FocusModeYouTubeMusicDelegate::SetFailureCallback(
    base::RepeatingClosure callback) {
  CHECK(callback);
  failure_callback_ = std::move(callback);
}

void FocusModeYouTubeMusicDelegate::ReservePlaylistForGetPlaylists(
    const std::string& playlist_id) {
  get_playlists_state_.reserved_playlist_id = playlist_id;
}

FocusModeYouTubeMusicDelegate::GetPlaylistsRequestState::
    GetPlaylistsRequestState() = default;
FocusModeYouTubeMusicDelegate::GetPlaylistsRequestState::
    ~GetPlaylistsRequestState() = default;

void FocusModeYouTubeMusicDelegate::GetPlaylistsRequestState::Reset() {
  for (auto& playlist_bucket : playlist_buckets) {
    playlist_bucket.clear();
  }
  playlists_to_query.clear();
  target_count = 0;
  count = 0;
  ResetDoneCallback();
}

void FocusModeYouTubeMusicDelegate::GetPlaylistsRequestState::
    ResetDoneCallback() {
  if (done_callback) {
    std::move(done_callback).Run({});
  }
  done_callback = base::NullCallback();
}

std::vector<FocusModeSoundsDelegate::Playlist>
FocusModeYouTubeMusicDelegate::GetPlaylistsRequestState::GetTopPlaylists() {
  std::vector<Playlist> results;
  results.reserve(kPlaylistNum);
  for (auto& playlist_bucket : playlist_buckets) {
    for (size_t i = 0;
         i < playlist_bucket.size() && results.size() < kPlaylistNum; i++) {
      // Skip the duplicate.
      if (base::ranges::find(results, playlist_bucket[i].id, &Playlist::id) !=
          results.end()) {
        continue;
      }
      results.emplace_back(playlist_bucket[i]);
    }
  }
  CHECK_EQ(results.size(), kPlaylistNum);
  return results;
}

FocusModeYouTubeMusicDelegate::GetNextTrackRequestState::
    GetNextTrackRequestState() = default;
FocusModeYouTubeMusicDelegate::GetNextTrackRequestState::
    ~GetNextTrackRequestState() = default;

void FocusModeYouTubeMusicDelegate::GetNextTrackRequestState::Reset() {
  last_playlist_id = std::string();
  last_queue_id = std::string();
  ResetDoneCallback();
}

void FocusModeYouTubeMusicDelegate::GetNextTrackRequestState::
    ResetDoneCallback() {
  if (done_callback) {
    std::move(done_callback).Run(std::nullopt);
  }
  done_callback = base::NullCallback();
}

void FocusModeYouTubeMusicDelegate::OnGetPlaylistDone(
    size_t bucket,
    google_apis::ApiErrorCode http_error_code,
    std::optional<youtube_music::Playlist> playlist) {
  if (http_error_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    if (http_error_code == google_apis::ApiErrorCode::HTTP_FORBIDDEN &&
        failure_callback_) {
      failure_callback_.Run();
    }
    get_playlists_state_.Reset();
    return;
  }

  if (!get_playlists_state_.done_callback) {
    return;
  }

  CHECK_LT(bucket, kYouTubeMusicPlaylistBucketCount);

  if (playlist.has_value()) {
    get_playlists_state_.playlist_buckets[bucket].emplace_back(
        playlist.value().name, playlist.value().title,
        playlist.value().image.url);
  }

  get_playlists_state_.count++;
  if (get_playlists_state_.count == get_playlists_state_.target_count) {
    const std::vector<Playlist>& results =
        get_playlists_state_.GetTopPlaylists();
    CHECK_GE(results.size(), kPlaylistNum);
    std::move(get_playlists_state_.done_callback).Run(results);
    get_playlists_state_.done_callback = base::NullCallback();
  }
}

void FocusModeYouTubeMusicDelegate::OnGetMusicSectionDone(
    size_t bucket,
    google_apis::ApiErrorCode http_error_code,
    std::optional<const std::vector<youtube_music::Playlist>> playlists) {
  if (http_error_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    if (http_error_code == google_apis::ApiErrorCode::HTTP_FORBIDDEN &&
        failure_callback_) {
      failure_callback_.Run();
    }
    get_playlists_state_.Reset();
    return;
  }

  if (!get_playlists_state_.done_callback) {
    return;
  }

  CHECK_LT(bucket, kYouTubeMusicPlaylistBucketCount);

  if (playlists.has_value()) {
    for (const auto& playlist : playlists.value()) {
      get_playlists_state_.playlist_buckets[bucket].emplace_back(
          playlist.name, playlist.title, playlist.image.url);
    }
  }

  get_playlists_state_.count++;
  if (get_playlists_state_.count == get_playlists_state_.target_count) {
    const std::vector<Playlist>& results =
        get_playlists_state_.GetTopPlaylists();
    CHECK_GE(results.size(), kPlaylistNum);
    std::move(get_playlists_state_.done_callback).Run(results);
    get_playlists_state_.done_callback = base::NullCallback();
  }
}

void FocusModeYouTubeMusicDelegate::OnNextTrackDone(
    const std::string& playlist_id,
    google_apis::ApiErrorCode http_error_code,
    std::optional<const youtube_music::PlaybackContext> playback_context) {
  if (http_error_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    if (http_error_code == google_apis::ApiErrorCode::HTTP_FORBIDDEN &&
        failure_callback_) {
      failure_callback_.Run();
    }
    next_track_state_.Reset();
    return;
  }

  if (!next_track_state_.done_callback) {
    return;
  }

  next_track_state_.last_playlist_id = playlist_id;
  next_track_state_.last_queue_id = playback_context->queue_name;

  std::optional<Track> result = std::nullopt;
  if (playback_context.has_value()) {
    result = Track(
        /*title=*/playback_context->track_title,
        /*artist=*/std::string(),
        /*source=*/
        base::StringPrintf(kYouTubeMusicSourceFormat, playlist_id.c_str()),
        /*thumbnail_url=*/playback_context->track_image.url,
        /*source_url=*/playback_context->stream_url,
        // YouTube Music requires playback reporting.
        /*enable_playback_reporting=*/true);
  }

  std::move(next_track_state_.done_callback).Run(result);
  next_track_state_.done_callback = base::NullCallback();
}

}  // namespace ash
