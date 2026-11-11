// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_playback_session.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "chrome/browser/readaloud/read_aloud_service.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"

namespace readaloud {

ReadAloudPlaybackSession::ReadAloudPlaybackSession(
    content::WebContents* web_contents,
    ReadAloudService* service,
    MediaSessionGetter media_session_getter)
    : content::WebContentsObserver(web_contents),
      service_(service),
      media_session_getter_(std::move(media_session_getter)) {}

ReadAloudPlaybackSession::~ReadAloudPlaybackSession() {
  if (content::MediaSession* session = GetMediaSessionIfExists()) {
    session->RemovePlayer(this, kPlayerId);
  }
}

bool ReadAloudPlaybackSession::NotifyPlaybackStarted() {
  is_paused_ = false;
  position_.end_of_media = false;
  if (ConsumePendingSessionCommand(SessionCommand::kResume)) {
    // `MediaSession` initiated the resume and already holds audio focus.
    is_registered_ = true;
    RefreshMediaSessionPosition();
    return true;
  }
  if (content::MediaSession* session = GetOrCreateMediaSession()) {
    MaybeUpdateReportedDuration();
    if (!session->AddPlayer(this, kPlayerId)) {
      is_paused_ = true;
      position_.ClearReported();
      return false;
    }
    is_registered_ = true;
    position_.last_reported_position = GetPosition(kPlayerId);
  }
  return true;
}

void ReadAloudPlaybackSession::NotifyPlaybackPaused(bool end_of_media) {
  is_paused_ = true;
  position_.end_of_media = end_of_media;
  if (!ConsumePendingSessionCommand(SessionCommand::kSuspend)) {
    if (content::MediaSession* session = GetMediaSessionIfExists()) {
      session->OnPlayerPaused(this, kPlayerId);
    }
  }
  RefreshMediaSessionPosition();
}

void ReadAloudPlaybackSession::NotifyPlaybackStopped() {
  is_paused_ = true;
  is_registered_ = false;
  position_ = PositionState();
  pending_session_command_ = SessionCommand::kNone;
  if (content::MediaSession* session = GetMediaSessionIfExists()) {
    session->RemovePlayer(this, kPlayerId);
  }
}

void ReadAloudPlaybackSession::OnPlaybackProgressChanged(
    base::TimeDelta elapsed,
    base::TimeDelta duration) {
  position_.elapsed = elapsed;
  if (duration.is_positive()) {
    position_.latest_received_duration = duration;
  }
  if (!is_registered_) {
    return;
  }
  const bool duration_changed = MaybeUpdateReportedDuration();
  if (!position_.HasValidDuration()) {
    return;
  }
  if (!position_.last_reported_position.has_value() || duration_changed) {
    RefreshMediaSessionPosition();
    return;
  }
  const base::TimeDelta drift =
      (std::min(position_.elapsed, position_.reported_duration) -
       position_.last_reported_position->GetPosition())
          .magnitude();
  if (is_paused_ ? drift.is_positive() : drift >= kPositionDriftThreshold) {
    RefreshMediaSessionPosition();
  }
}

void ReadAloudPlaybackSession::OnPlaybackRateChanged(float rate) {
  if (!std::isfinite(rate) || rate <= 0.0f) {
    return;
  }
  const double clamped_rate =
      std::clamp(rate, kMinPlaybackRate, kMaxPlaybackRate);
  if (playback_rate_ == clamped_rate) {
    return;
  }
  playback_rate_ = clamped_rate;
  RefreshMediaSessionPosition();
}

bool ReadAloudPlaybackSession::ConsumePendingSessionCommand(
    SessionCommand command) {
  return std::exchange(pending_session_command_, SessionCommand::kNone) ==
         command;
}

bool ReadAloudPlaybackSession::MaybeUpdateReportedDuration() {
  if (!position_.latest_received_duration.is_positive() ||
      position_.reported_duration == position_.latest_received_duration) {
    return false;
  }
  const base::TimeTicks now = base::TimeTicks::Now();
  if (!position_.last_duration_update_time.is_null() &&
      now - position_.last_duration_update_time < kMinDurationUpdateInterval) {
    return false;
  }
  position_.reported_duration = position_.latest_received_duration;
  position_.last_duration_update_time = now;
  return true;
}

void ReadAloudPlaybackSession::RefreshMediaSessionPosition() {
  if (!is_registered_) {
    return;
  }
  content::MediaSession* session = GetMediaSessionIfExists();
  if (!session) {
    return;
  }
  MaybeUpdateReportedDuration();
  if (!position_.HasValidDuration()) {
    return;
  }
  // `content::MediaSession` does not expose
  // `RebuildAndNotifyMediaPositionChanged()`; calling `AddPlayer()` for an
  // already-registered player updates its entry in-place and triggers a
  // position rebuild.
  if (session->AddPlayer(this, kPlayerId)) {
    position_.last_reported_position = GetPosition(kPlayerId);
  }
}

content::MediaSession* ReadAloudPlaybackSession::GetOrCreateMediaSession() {
  if (!web_contents()) {
    return nullptr;
  }
  if (media_session_getter_) {
    return media_session_getter_.Run(web_contents());
  }
  return content::MediaSession::Get(web_contents());
}

content::MediaSession* ReadAloudPlaybackSession::GetMediaSessionIfExists() {
  if (!web_contents()) {
    return nullptr;
  }
  if (media_session_getter_) {
    return media_session_getter_.Run(web_contents());
  }
  return content::MediaSession::GetIfExists(web_contents());
}

void ReadAloudPlaybackSession::OnSuspend(int player_id,
                                         bool triggered_by_user) {
  if (player_id != kPlayerId) {
    return;
  }
  pending_session_command_ = SessionCommand::kSuspend;
  service_->OnSessionSuspended();
}

void ReadAloudPlaybackSession::OnResume(int player_id, bool triggered_by_user) {
  if (player_id != kPlayerId) {
    return;
  }
  pending_session_command_ = SessionCommand::kResume;
  service_->OnSessionResumed();
}

void ReadAloudPlaybackSession::OnSeekForward(int player_id,
                                             base::TimeDelta seek_time) {
  if (player_id != kPlayerId) {
    return;
  }
  service_->SeekRelative(seek_time);
}

void ReadAloudPlaybackSession::OnSeekBackward(int player_id,
                                              base::TimeDelta seek_time) {
  if (player_id != kPlayerId) {
    return;
  }
  service_->SeekRelative(-seek_time);
}

void ReadAloudPlaybackSession::OnSeekTo(int player_id,
                                        base::TimeDelta seek_time) {
  if (player_id != kPlayerId) {
    return;
  }
  service_->Seek(seek_time);
}

std::optional<media_session::MediaPosition>
ReadAloudPlaybackSession::GetPosition(int player_id) const {
  if (player_id != kPlayerId || !position_.HasValidDuration()) {
    return std::nullopt;
  }
  return media_session::MediaPosition(
      /*playback_rate=*/is_paused_ ? 0.0 : playback_rate_,
      /*duration=*/position_.reported_duration,
      /*position=*/std::min(position_.elapsed, position_.reported_duration),
      /*end_of_media=*/position_.end_of_media);
}

content::RenderFrameHost* ReadAloudPlaybackSession::render_frame_host() const {
  return web_contents() ? web_contents()->GetPrimaryMainFrame() : nullptr;
}

bool ReadAloudPlaybackSession::HasAudio(int player_id) const {
  return true;
}

bool ReadAloudPlaybackSession::IsPaused(int player_id) const {
  return is_paused_;
}

}  // namespace readaloud
