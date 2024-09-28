// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_youtube_music_delegate.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_retry_util.h"
#include "ash/system/focus_mode/focus_mode_util.h"
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

constexpr bool IsErrorFatal(google_apis::ApiErrorCode http_error_code) {
  return http_error_code == google_apis::ApiErrorCode::HTTP_BAD_REQUEST;
}

bool ShouldRetryRequest(google_apis::ApiErrorCode http_error_code,
                        int retry_index) {
  if (http_error_code == 429 && retry_index < kMaxRetryTooManyRequests) {
    return true;
  }

  return ShouldRetryHttpError(http_error_code) &&
         retry_index < kMaxRetryOverall;
}

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

  if (ContainsFatalError()) {
    std::move(callback).Run({});
    return;
  }

  next_track_state_.retry_state.Reset();
  next_track_state_.ResetDoneCallback();
  next_track_state_.done_callback = std::move(callback);

  GetNextTrackInternal(playlist_id);
}

void FocusModeYouTubeMusicDelegate::GetPlaylists(
    FocusModeSoundsDelegate::PlaylistsCallback callback) {
  CHECK(callback);
  get_playlists_state_.Reset();

  if (ContainsFatalError()) {
    std::move(callback).Run({});
    return;
  }

  // Cache the done callback, add focus supermix/reserved playlist to the to-do
  // list, and update the total number of API request to run.
  get_playlists_state_.done_callback = std::move(callback);
  const bool playlist_reserved =
      get_playlists_state_.reserved_playlist_id.has_value() &&
      get_playlists_state_.reserved_playlist_id.value() !=
          kFocusSupermixPlaylistId;
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

void FocusModeYouTubeMusicDelegate::ReportPlayback(
    const youtube_music::PlaybackData& playback_data) {
  // Check for token and see if it has sufficient data for the reporting
  // request.
  const auto state_iterator = report_playback_states_.find(playback_data.url);
  if (state_iterator == report_playback_states_.end() ||
      !state_iterator->second.get()) {
    return;
  }

  if (ContainsFatalError()) {
    return;
  }

  ReportPlaybackRequestState& state = *state_iterator->second;
  state.retry_state.Reset();
  state.playback_state = playback_data.state;
  if (state.staged_playback_data.has_value() &&
      state.staged_playback_data->CanAggregateWithNewData(playback_data)) {
    state.staged_playback_data.value().AggregateWithNewData(playback_data);
  } else {
    state.staged_playback_data = playback_data;
  }

  ReportPlaybackInternal(playback_data.url);
}

void FocusModeYouTubeMusicDelegate::SetNoPremiumCallback(
    base::RepeatingClosure callback) {
  CHECK(callback);
  no_premium_callback_ = std::move(callback);
}

void FocusModeYouTubeMusicDelegate::SetErrorCallback(
    ApiErrorCallback callback) {
  CHECK(callback);
  error_callback_ = std::move(callback);
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
                     weak_factory_.GetWeakPtr(),
                     /*start_time=*/base::Time::Now(), type));
}

void FocusModeYouTubeMusicDelegate::OnGetPlaylistDone(
    const base::Time start_time,
    const GetPlaylistsRequestState::PlaylistType type,
    base::expected<youtube_music::Playlist,
                   google_apis::youtube_music::ApiError> playlist) {
  google_apis::ApiErrorCode http_error_code = playlist.has_value()
                                                  ? google_apis::HTTP_SUCCESS
                                                  : playlist.error().error_code;
  const std::string method = "YouTubeMusic.GetPlaylist";
  focus_mode_util::RecordHistogramForApiStatus(method, http_error_code);
  focus_mode_util::RecordHistogramForApiLatency(method,
                                                base::Time::Now() - start_time);

  if (!get_playlists_state_.done_callback) {
    return;
  }

  const size_t bucket = static_cast<size_t>(type);
  CHECK_LT(bucket, kYouTubeMusicPlaylistBucketCount);

  FocusModeRetryState& retry_state = get_playlists_state_.retry_states[bucket];
  if (playlist.has_value()) {
    get_playlists_state_.playlist_buckets[bucket].emplace_back(
        playlist.value().name, playlist.value().title,
        playlist.value().image.url);
  } else {
    if (ShouldRetryRequest(http_error_code, retry_state.retry_index)) {
      retry_state.retry_index++;
      retry_state.timer.Start(
          FROM_HERE,
          GetExponentialBackoffRetryWaitTime(retry_state.retry_index),
          base::BindOnce(&FocusModeYouTubeMusicDelegate::GetPlaylistInternal,
                         weak_factory_.GetWeakPtr(), type));
      return;
    }

    // Handle forbidden error. No need to retry.
    if (http_error_code == google_apis::ApiErrorCode::HTTP_FORBIDDEN) {
      // Notify UI about no premium subscription.
      if (no_premium_callback_) {
        no_premium_callback_.Run();
      }

      // Bail gracefully.
      get_playlists_state_.Reset();
    } else {
      // Error will not be retried we are giving up.
      ApiErrorEncountered(
          {IsErrorFatal(http_error_code), playlist.error().error_message});
    }
  }

  focus_mode_util::RecordHistogramForApiRetryCount(method,
                                                   retry_state.retry_index);
  focus_mode_util::RecordHistogramForApiResult(
      method,
      /*successful=*/playlist.has_value());

  get_playlists_state_.count++;
  MaybeReportBackPlaylists();
}

void FocusModeYouTubeMusicDelegate::GetMusicSectionInternal() {
  youtube_music_controller_->GetMusicSection(base::BindOnce(
      &FocusModeYouTubeMusicDelegate::OnGetMusicSectionDone,
      weak_factory_.GetWeakPtr(), /*start_time=*/base::Time::Now()));
}

void FocusModeYouTubeMusicDelegate::OnGetMusicSectionDone(
    const base::Time start_time,
    base::expected<const std::vector<youtube_music::Playlist>,
                   google_apis::youtube_music::ApiError> playlists) {
  google_apis::ApiErrorCode http_error_code =
      playlists.has_value() ? google_apis::HTTP_SUCCESS
                            : playlists.error().error_code;
  const std::string method = "YouTubeMusic.GetMusicSection";
  focus_mode_util::RecordHistogramForApiStatus(method, http_error_code);
  focus_mode_util::RecordHistogramForApiLatency(method,
                                                base::Time::Now() - start_time);

  if (!get_playlists_state_.done_callback) {
    return;
  }

  const size_t bucket =
      static_cast<size_t>(GetPlaylistsRequestState::PlaylistType::kFocusIntent);
  CHECK_LT(bucket, kYouTubeMusicPlaylistBucketCount);

  FocusModeRetryState& retry_state = get_playlists_state_.retry_states[bucket];
  if (playlists.has_value()) {
    for (const auto& playlist : playlists.value()) {
      get_playlists_state_.playlist_buckets[bucket].emplace_back(
          playlist.name, playlist.title, playlist.image.url);
    }
  } else {
    // Handle general HTTP errors. Retry if needed.
    if (ShouldRetryRequest(http_error_code, retry_state.retry_index)) {
      retry_state.retry_index++;
      retry_state.timer.Start(
          FROM_HERE,
          GetExponentialBackoffRetryWaitTime(retry_state.retry_index),
          base::BindOnce(
              &FocusModeYouTubeMusicDelegate::GetMusicSectionInternal,
              weak_factory_.GetWeakPtr()));
      return;
    }

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

    // Error will not be retried we are giving up.
    ApiErrorEncountered(
        {IsErrorFatal(http_error_code), playlists.error().error_message});
  }

  // Do not record retry count and final result for non-premium users.
  if (http_error_code != google_apis::ApiErrorCode::HTTP_FORBIDDEN) {
    focus_mode_util::RecordHistogramForApiRetryCount(method,
                                                     retry_state.retry_index);
    focus_mode_util::RecordHistogramForApiResult(
        method,
        /*successful=*/playlists.has_value() && !playlists.value().empty());
  }

  get_playlists_state_.count++;
  MaybeReportBackPlaylists();
}

void FocusModeYouTubeMusicDelegate::MaybeReportBackPlaylists() {
  if (get_playlists_state_.count != get_playlists_state_.target_count) {
    return;
  }

  const std::vector<Playlist>& results = get_playlists_state_.GetTopPlaylists();
  if (results.size() == kFocusModePlaylistViewsNum) {
    std::move(get_playlists_state_.done_callback).Run(results);
    RequestSuccessful();
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
                       weak_factory_.GetWeakPtr(),
                       /*start_time=*/base::Time::Now(), /*prepare==*/true,
                       playlist_id));
  } else {
    youtube_music_controller_->PlaybackQueueNext(
        next_track_state_.last_queue_id,
        base::BindOnce(&FocusModeYouTubeMusicDelegate::OnNextTrackDone,
                       weak_factory_.GetWeakPtr(),
                       /*start_time=*/base::Time::Now(), /*prepare==*/false,
                       playlist_id));
  }
}

void FocusModeYouTubeMusicDelegate::OnNextTrackDone(
    const base::Time start_time,
    const bool prepare,
    const std::string& playlist_id,
    base::expected<const youtube_music::PlaybackContext,
                   google_apis::youtube_music::ApiError> playback_context) {
  google_apis::ApiErrorCode http_error_code =
      playback_context.has_value() ? google_apis::HTTP_SUCCESS
                                   : playback_context.error().error_code;

  const std::string method = prepare ? "YouTubeMusic.PlaybackQueuePrepare"
                                     : "YouTubeMusic.PlaybackQueueNext";
  focus_mode_util::RecordHistogramForApiStatus(method, http_error_code);
  focus_mode_util::RecordHistogramForApiLatency(method,
                                                base::Time::Now() - start_time);

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
      ApiErrorEncountered({false, playback_context.error().error_message});
      return;
    }

    // Too many request error. Retry if needed.
    if (ShouldRetryRequest(http_error_code,
                           next_track_state_.retry_state.retry_index)) {
      next_track_state_.retry_state.timer.Start(
          FROM_HERE,
          GetExponentialBackoffRetryWaitTime(
              next_track_state_.retry_state.retry_index),
          base::BindOnce(&FocusModeYouTubeMusicDelegate::GetNextTrackInternal,
                         weak_factory_.GetWeakPtr(), playlist_id));
      return;
    }

    // Other unhandled HTTP errors or maximum retry reached. Bail gracefully.
    focus_mode_util::RecordHistogramForApiRetryCount(
        method, next_track_state_.retry_state.retry_index);
    focus_mode_util::RecordHistogramForApiResult(method,
                                                 /*successful=*/false);
    std::move(next_track_state_.done_callback).Run(std::nullopt);
    next_track_state_.Reset();

    // Report the error.
    ApiErrorEncountered({IsErrorFatal(http_error_code),
                         playback_context.error().error_message});
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
      focus_mode_util::RecordHistogramForApiRetryCount(
          method, next_track_state_.retry_state.retry_index);
      focus_mode_util::RecordHistogramForApiResult(method,
                                                   /*successful=*/false);
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
    report_playback_states_.emplace(
        playback_context->stream_url,
        std::make_unique<ReportPlaybackRequestState>());
    report_playback_states_.at(playback_context->stream_url)->token =
        playback_context->playback_reporting_token;
  }

  focus_mode_util::RecordHistogramForApiRetryCount(
      method, next_track_state_.retry_state.retry_index);
  focus_mode_util::RecordHistogramForApiResult(
      method,
      /*successful=*/result.has_value());

  std::move(next_track_state_.done_callback).Run(result);
  RequestSuccessful();
  next_track_state_.done_callback = base::NullCallback();

  // For a successful request, reset the retry state so that it could handle
  // failure correctly going forward.
  next_track_state_.retry_state.Reset();
}

void FocusModeYouTubeMusicDelegate::ReportPlaybackInternal(const GURL& url) {
  const auto state_iterator = report_playback_states_.find(url);
  if (state_iterator == report_playback_states_.end() ||
      !state_iterator->second.get()) {
    return;
  }

  ReportPlaybackRequestState& state = *state_iterator->second;
  youtube_music_controller_->ReportPlayback(
      state.token, state.staged_playback_data.value(),
      base::BindOnce(&FocusModeYouTubeMusicDelegate::OnReportPlaybackDone,
                     weak_factory_.GetWeakPtr(),
                     /*start_time=*/base::Time::Now(), url));
}

void FocusModeYouTubeMusicDelegate::OnReportPlaybackDone(
    const base::Time start_time,
    const GURL& url,
    base::expected<const std::string, google_apis::youtube_music::ApiError>
        new_playback_reporting_token) {
  google_apis::ApiErrorCode http_error_code =
      new_playback_reporting_token.has_value()
          ? google_apis::HTTP_SUCCESS
          : new_playback_reporting_token.error().error_code;

  const std::string method = "YouTubeMusic.ReportPlayback";
  focus_mode_util::RecordHistogramForApiStatus(method, http_error_code);
  focus_mode_util::RecordHistogramForApiLatency(method,
                                                base::Time::Now() - start_time);

  const auto state_iterator = report_playback_states_.find(url);
  if (state_iterator == report_playback_states_.end() ||
      !state_iterator->second.get()) {
    return;
  }

  ReportPlaybackRequestState& state = *state_iterator->second;
  const bool last_report =
      state.playback_state == youtube_music::PlaybackState::kEnded ||
      state.playback_state == youtube_music::PlaybackState::kSwitchedToNext;
  if (http_error_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    // Handle forbidden error. No need for further attempts.
    if (http_error_code == google_apis::ApiErrorCode::HTTP_FORBIDDEN) {
      if (no_premium_callback_) {
        no_premium_callback_.Run();
      }

      // Bail gracefully.
      report_playback_states_.erase(url);
      return;
    }

    if (ShouldRetryRequest(http_error_code, state.retry_state.retry_index)) {
      state.retry_state.retry_index++;
      state.retry_state.timer.Start(
          FROM_HERE, kWaitTimeTooManyRequests,
          base::BindOnce(&FocusModeYouTubeMusicDelegate::ReportPlaybackInternal,
                         weak_factory_.GetWeakPtr(), url));
      return;
    }

    // Bail gracefully. If it's the last report of the track, remove its local
    // state; otherwise, reset the retry state for the next report so that the
    // staged data could be aggregated into the new incoming data and still be
    // reported.
    focus_mode_util::RecordHistogramForApiRetryCount(
        method, state.retry_state.retry_index);
    focus_mode_util::RecordHistogramForApiResult(method,
                                                 /*successful=*/false);
    if (last_report) {
      report_playback_states_.erase(url);
    } else {
      state.retry_state.Reset();
    }

    // Error will not be retried we are giving up.
    ApiErrorEncountered({IsErrorFatal(http_error_code),
                         new_playback_reporting_token.error().error_message});
    return;
  }

  focus_mode_util::RecordHistogramForApiRetryCount(
      method, state.retry_state.retry_index);
  focus_mode_util::RecordHistogramForApiResult(method,
                                               /*successful=*/true);

  // When a track is completed, clear the local data.
  if (last_report) {
    report_playback_states_.erase(url);
    return;
  }

  // Refresh the reports.playback token since we have a new one. Please note,
  // the API server may return empty tokens when a track is completed.
  if (new_playback_reporting_token.has_value() &&
      !new_playback_reporting_token.value().empty()) {
    state.token = new_playback_reporting_token.value();
  }

  // Commit finished playback data from staging area.
  state.staged_playback_data.reset();

  // For a successful request, reset the retry state so that it could handle
  // failure correctly going forward.
  state.retry_state.Reset();
}

bool FocusModeYouTubeMusicDelegate::ContainsFatalError() const {
  return last_error_.has_value() && last_error_->fatal;
}

void FocusModeYouTubeMusicDelegate::RequestSuccessful() {
  if (ContainsFatalError()) {
    // Fatal errors cannot be cleared.
    return;
  }
  last_error_.reset();
}

void FocusModeYouTubeMusicDelegate::ApiErrorEncountered(
    FocusModeApiError api_error) {
  if (ContainsFatalError()) {
    // Only the first fatal error is emitted.
    return;
  }

  last_error_ = api_error;
  if (error_callback_) {
    error_callback_.Run(api_error);
  }
}

}  // namespace ash
