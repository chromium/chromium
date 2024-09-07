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
#include "ash/system/focus_mode/focus_mode_retry_util.h"
#include "ash/system/focus_mode/sounds/sound_section_view.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "google_apis/common/api_error_codes.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kFocusSupermixPlaylistId[] =
    "playlists/RDTMAK5uy_l3TXw3uC_sIHl4m6RMGqCyKKd2D2_pv28";
constexpr char kYouTubeMusicSourceFormat[] = "YouTube Music ᐧ %s";
constexpr char kYouTubeMusicTrackNotExplicit[] = "EXPLICIT_TYPE_NOT_EXPLICIT";

}  // namespace

FocusModeYouTubeMusicDelegate::FocusModeYouTubeMusicDelegate() {
  youtube_music_controller_ =
      std::make_unique<youtube_music::YouTubeMusicController>();
}

FocusModeYouTubeMusicDelegate::~FocusModeYouTubeMusicDelegate() = default;

void FocusModeYouTubeMusicDelegate::GetNextTrack(
    const std::string& playlist_id,
    FocusModeSoundsDelegate::TrackCallback callback) {
  CHECK(callback);
  next_track_state_.retry_state.Reset();
  next_track_state_.ResetDoneCallback();
  next_track_state_.done_callback = std::move(callback);

  GetNextTrackInternal(playlist_id);
}

void FocusModeYouTubeMusicDelegate::GetPlaylists(
    FocusModeSoundsDelegate::PlaylistsCallback callback) {
  CHECK(callback);
  get_playlists_state_.Reset();

  // Cache the done callback, add focus supermix/reserved playlist to the to-do
  // list, and update the total number of API request to run.
  get_playlists_state_.done_callback = std::move(callback);
  const bool playlist_reserved =
      get_playlists_state_.reserved_playlist_id.has_value();
  get_playlists_state_.target_count = playlist_reserved ? 3 : 2;

  // Invoke sub requests. Please note, sub request failures are more permissive
  // than other requests. As long as we can get four playlists, it will be
  // treated as a successful overall attempt. This is especially robust when a
  // certain playlist is deleted so that one single failed sub request does not
  // affect the overall ability to fetch playlists.
  GetPlaylistInternal(GetPlaylistsRequestState::PlaylistType::kFocusSuperMix);
  if (playlist_reserved) {
    GetPlaylistInternal(GetPlaylistsRequestState::PlaylistType::kReserved);
  }
  GetMusicSectionInternal();
}

bool FocusModeYouTubeMusicDelegate::ReportPlayback(
    const youtube_music::PlaybackData& playback_data) {
  // Check for token and see if it has sufficient data for the reporting
  // request.
  if (report_playback_state_.url_to_token.find(playback_data.url) ==
      report_playback_state_.url_to_token.end()) {
    return false;
  }

  report_playback_state_.url_to_playback_state.insert(
      {playback_data.url, playback_data.state});
  const std::string& playback_reporting_token =
      report_playback_state_.url_to_token[playback_data.url];

  return youtube_music_controller_->ReportPlayback(
      playback_reporting_token, playback_data,
      base::BindOnce(&FocusModeYouTubeMusicDelegate::OnReportPlaybackDone,
                     weak_factory_.GetWeakPtr(), playback_data.url));
}

void FocusModeYouTubeMusicDelegate::SetNoPremiumCallback(
    base::RepeatingClosure callback) {
  CHECK(callback);
  no_premium_callback_ = std::move(callback);
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
  for (auto& retry_state : retry_states) {
    retry_state.Reset();
  }
  target_count = 0;
  count = 0;
  ResetDoneCallback();
}

void FocusModeYouTubeMusicDelegate::GetPlaylistsRequestState::
    ResetDoneCallback() {
  if (done_callback) {
    std::move(done_callback).Run(/*playlists=*/{});
  }
  done_callback = base::NullCallback();
}

std::vector<FocusModeSoundsDelegate::Playlist>
FocusModeYouTubeMusicDelegate::GetPlaylistsRequestState::GetTopPlaylists() {
  std::vector<Playlist> results;
  results.reserve(kFocusModePlaylistViewsNum);
  for (auto& playlist_bucket : playlist_buckets) {
    for (size_t i = 0; i < playlist_bucket.size() &&
                       results.size() < kFocusModePlaylistViewsNum;
         i++) {
      // Skip the duplicate.
      if (base::ranges::find(results, playlist_bucket[i].id, &Playlist::id) !=
          results.end()) {
        continue;
      }
      results.emplace_back(playlist_bucket[i]);
    }
  }
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
  retry_state.Reset();
}

void FocusModeYouTubeMusicDelegate::GetNextTrackRequestState::
    ResetDoneCallback() {
  if (done_callback) {
    std::move(done_callback).Run(std::nullopt);
  }
  done_callback = base::NullCallback();
}

FocusModeYouTubeMusicDelegate::ReportPlaybackRequestState::
    ReportPlaybackRequestState() = default;
FocusModeYouTubeMusicDelegate::ReportPlaybackRequestState::
    ~ReportPlaybackRequestState() = default;

bool FocusModeYouTubeMusicDelegate::ReportPlaybackRequestState::
    CanReportPlaybackForUrl(const GURL& url) {
  return url_to_playback_state.find(url) != url_to_playback_state.end() &&
         url_to_token.find(url) != url_to_token.end();
}

void FocusModeYouTubeMusicDelegate::GetPlaylistInternal(
    const GetPlaylistsRequestState::PlaylistType type) {
  std::string playlist_id;
  if (type == GetPlaylistsRequestState::PlaylistType::kReserved) {
    CHECK(get_playlists_state_.reserved_playlist_id.has_value());
    playlist_id = get_playlists_state_.reserved_playlist_id.value();
  } else {
    playlist_id = kFocusSupermixPlaylistId;
  }

  youtube_music_controller_->GetPlaylist(
      playlist_id,
      base::BindOnce(&FocusModeYouTubeMusicDelegate::OnGetPlaylistDone,
                     weak_factory_.GetWeakPtr(), type));
}

void FocusModeYouTubeMusicDelegate::OnGetPlaylistDone(
    const GetPlaylistsRequestState::PlaylistType type,
    google_apis::ApiErrorCode http_error_code,
    std::optional<youtube_music::Playlist> playlist) {
  if (!get_playlists_state_.done_callback) {
    return;
  }

  const size_t bucket = static_cast<size_t>(type);
  CHECK_LT(bucket, kYouTubeMusicPlaylistBucketCount);

  if (http_error_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    // Handle forbidden error. No need to retry.
    if (http_error_code == google_apis::ApiErrorCode::HTTP_FORBIDDEN) {
      // Notify UI about no premium subscription.
      if (no_premium_callback_) {
        no_premium_callback_.Run();
      }

      // Bail gracefully.
      get_playlists_state_.Reset();
      return;
    }

    // Handle too many request error. Retry if needed.
    FocusModeRetryState& retry_state =
        get_playlists_state_.retry_states[bucket];
    if (http_error_code == 429 &&
        retry_state.retry_index < kMaxRetryTooManyRequests) {
      retry_state.retry_index++;
      retry_state.timer.Start(
          FROM_HERE, kWaitTimeTooManyRequests,
          base::BindOnce(&FocusModeYouTubeMusicDelegate::GetPlaylistInternal,
                         weak_factory_.GetWeakPtr(), type));
      return;
    }

    // Handle general HTTP errors. Retry if needed.
    if (ShouldRetryHttpError(http_error_code) &&
        retry_state.retry_index < kMaxRetryOverall) {
      retry_state.retry_index++;
      retry_state.timer.Start(
          FROM_HERE,
          GetExponentialBackoffRetryWaitTime(retry_state.retry_index),
          base::BindOnce(&FocusModeYouTubeMusicDelegate::GetPlaylistInternal,
                         weak_factory_.GetWeakPtr(), type));
      return;
    }
  }

  if (playlist.has_value()) {
    get_playlists_state_.playlist_buckets[bucket].emplace_back(
        playlist.value().name, playlist.value().title,
        playlist.value().image.url);
  }

  get_playlists_state_.count++;
  MaybeReportBackPlaylists();
}

void FocusModeYouTubeMusicDelegate::GetMusicSectionInternal() {
  youtube_music_controller_->GetMusicSection(
      base::BindOnce(&FocusModeYouTubeMusicDelegate::OnGetMusicSectionDone,
                     weak_factory_.GetWeakPtr()));
}

void FocusModeYouTubeMusicDelegate::OnGetMusicSectionDone(
    google_apis::ApiErrorCode http_error_code,
    std::optional<const std::vector<youtube_music::Playlist>> playlists) {
  if (!get_playlists_state_.done_callback) {
    return;
  }

  const size_t bucket =
      static_cast<size_t>(GetPlaylistsRequestState::PlaylistType::kFocusIntent);
  CHECK_LT(bucket, kYouTubeMusicPlaylistBucketCount);

  if (http_error_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    // Handle forbidden error. No need to retry.
    if (http_error_code == google_apis::ApiErrorCode::HTTP_FORBIDDEN) {
      // Notify UI about no premium subscription.
      if (no_premium_callback_) {
        no_premium_callback_.Run();
      }

      // Bail gracefully.
      get_playlists_state_.Reset();
      return;
    }

    // Handle too many request error. Retry if needed.
    FocusModeRetryState& retry_state =
        get_playlists_state_.retry_states[bucket];
    if (http_error_code == 429 &&
        retry_state.retry_index < kMaxRetryTooManyRequests) {
      retry_state.retry_index++;
      retry_state.timer.Start(
          FROM_HERE, kWaitTimeTooManyRequests,
          base::BindOnce(
              &FocusModeYouTubeMusicDelegate::GetMusicSectionInternal,
              weak_factory_.GetWeakPtr()));
      return;
    }

    // Handle general HTTP errors. Retry if needed.
    if (ShouldRetryHttpError(http_error_code) &&
        retry_state.retry_index < kMaxRetryOverall) {
      retry_state.retry_index++;
      retry_state.timer.Start(
          FROM_HERE,
          GetExponentialBackoffRetryWaitTime(retry_state.retry_index),
          base::BindOnce(
              &FocusModeYouTubeMusicDelegate::GetMusicSectionInternal,
              weak_factory_.GetWeakPtr()));
      return;
    }
  }

  if (playlists.has_value()) {
    for (const auto& playlist : playlists.value()) {
      get_playlists_state_.playlist_buckets[bucket].emplace_back(
          playlist.name, playlist.title, playlist.image.url);
    }
  }

  get_playlists_state_.count++;
  MaybeReportBackPlaylists();
}

void FocusModeYouTubeMusicDelegate::MaybeReportBackPlaylists() {
  if (get_playlists_state_.count != get_playlists_state_.target_count) {
    return;
  }

  const std::vector<Playlist>& results = get_playlists_state_.GetTopPlaylists();
  if (results.size() >= kFocusModePlaylistViewsNum) {
    std::move(get_playlists_state_.done_callback).Run(results);
    get_playlists_state_.done_callback = base::NullCallback();
  }

  get_playlists_state_.Reset();
}

void FocusModeYouTubeMusicDelegate::GetNextTrackInternal(
    const std::string& playlist_id) {
  if (next_track_state_.last_playlist_id != playlist_id) {
    youtube_music_controller_->PlaybackQueuePrepare(
        playlist_id,
        base::BindOnce(&FocusModeYouTubeMusicDelegate::OnNextTrackDone,
                       weak_factory_.GetWeakPtr(), playlist_id));
  } else {
    youtube_music_controller_->PlaybackQueueNext(
        next_track_state_.last_queue_id,
        base::BindOnce(&FocusModeYouTubeMusicDelegate::OnNextTrackDone,
                       weak_factory_.GetWeakPtr(), playlist_id));
  }
}

void FocusModeYouTubeMusicDelegate::OnNextTrackDone(
    const std::string& playlist_id,
    google_apis::ApiErrorCode http_error_code,
    std::optional<const youtube_music::PlaybackContext> playback_context) {
  if (!next_track_state_.done_callback) {
    return;
  }

  if (http_error_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    // Handle forbidden error. No need to retry.
    if (http_error_code == google_apis::ApiErrorCode::HTTP_FORBIDDEN) {
      // Notify UI about no premium subscription.
      if (no_premium_callback_) {
        no_premium_callback_.Run();
      }

      // Bail gracefully.
      std::move(next_track_state_.done_callback).Run(std::nullopt);
      next_track_state_.Reset();
      return;
    }

    // Handle too many request error.
    if (http_error_code == 429) {
      // Retry if needed.
      if (next_track_state_.retry_state.retry_index <
          kMaxRetryTooManyRequests) {
        next_track_state_.retry_state.retry_index++;
        next_track_state_.retry_state.timer.Start(
            FROM_HERE, kWaitTimeTooManyRequests,
            base::BindOnce(&FocusModeYouTubeMusicDelegate::GetNextTrackInternal,
                           weak_factory_.GetWeakPtr(), playlist_id));
        return;
      }

      // Max number of retries reached. Bail gracefully.
      std::move(next_track_state_.done_callback).Run(std::nullopt);
      next_track_state_.Reset();
      return;
    }

    // Handle general HTTP errors.
    if (ShouldRetryHttpError(http_error_code)) {
      // Retry if needed.
      if (next_track_state_.retry_state.retry_index < kMaxRetryOverall) {
        next_track_state_.retry_state.retry_index++;
        next_track_state_.retry_state.timer.Start(
            FROM_HERE,
            GetExponentialBackoffRetryWaitTime(
                next_track_state_.retry_state.retry_index),
            base::BindOnce(&FocusModeYouTubeMusicDelegate::GetNextTrackInternal,
                           weak_factory_.GetWeakPtr(), playlist_id));
        return;
      }

      // Max number of retries reached. Bail gracefully.
      std::move(next_track_state_.done_callback).Run(std::nullopt);
      next_track_state_.Reset();
      return;
    }

    // Other unhandled HTTP errors. Bail gracefully.
    std::move(next_track_state_.done_callback).Run(std::nullopt);
    next_track_state_.Reset();
    return;
  }

  next_track_state_.last_playlist_id = playlist_id;
  next_track_state_.last_queue_id = playback_context->queue_name;

  std::optional<Track> result = std::nullopt;
  if (playback_context.has_value()) {
    // Handle explicit track.
    if (playback_context->track_explicit_type_ !=
        kYouTubeMusicTrackNotExplicit) {
      // Retry if needed.
      if (next_track_state_.retry_state.retry_index < kMaxRetryExplicitTrack) {
        next_track_state_.retry_state.retry_index++;
        next_track_state_.retry_state.timer.Start(
            FROM_HERE, kWaitTimeExplicitTrack,
            base::BindOnce(&FocusModeYouTubeMusicDelegate::GetNextTrackInternal,
                           weak_factory_.GetWeakPtr(), playlist_id));
        return;
      }

      // Max number of retries reached. Bail gracefully.
      std::move(next_track_state_.done_callback).Run(std::nullopt);
      next_track_state_.Reset();
      return;
    }

    result = Track(
        /*title=*/playback_context->track_title,
        /*artist=*/playback_context->track_artists,
        /*source=*/
        base::StringPrintf(kYouTubeMusicSourceFormat, playlist_id.c_str()),
        /*thumbnail_url=*/playback_context->track_image.url,
        /*source_url=*/playback_context->stream_url,
        // YouTube Music requires playback reporting.
        /*enable_playback_reporting=*/true);
    report_playback_state_.url_to_token[playback_context->stream_url] =
        playback_context->playback_reporting_token;
  }

  std::move(next_track_state_.done_callback).Run(result);
  next_track_state_.done_callback = base::NullCallback();

  // For a successful request, reset the retry state so that it could handle
  // failure correctly going forward.
  next_track_state_.retry_state.Reset();
}

void FocusModeYouTubeMusicDelegate::OnReportPlaybackDone(
    const GURL& url,
    google_apis::ApiErrorCode http_error_code,
    std::optional<const std::string> new_playback_reporting_token) {
  if (http_error_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    if (http_error_code == google_apis::ApiErrorCode::HTTP_FORBIDDEN &&
        no_premium_callback_) {
      no_premium_callback_.Run();
    }
    // TODO(b/354240276): Add more error handling and retries.
    return;
  }

  if (!report_playback_state_.CanReportPlaybackForUrl(url)) {
    return;
  }

  // Refresh the reports.playback token since we have a new one. Please note,
  // the API server may return empty tokens when a track is completed.
  if (new_playback_reporting_token.has_value() &&
      !new_playback_reporting_token.value().empty()) {
    report_playback_state_.url_to_token[url] =
        new_playback_reporting_token.value();
  }

  // When a track is completed, clear the local data.
  if (report_playback_state_.url_to_playback_state.at(url) ==
          youtube_music::PlaybackState::kEnded ||
      report_playback_state_.url_to_playback_state.at(url) ==
          youtube_music::PlaybackState::kSwitchedToNext) {
    report_playback_state_.url_to_playback_state.erase(url);
    report_playback_state_.url_to_token.erase(url);
  }
}

}  // namespace ash
