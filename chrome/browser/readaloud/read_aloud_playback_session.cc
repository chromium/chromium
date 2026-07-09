// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_playback_session.h"

#include "chrome/browser/readaloud/read_aloud_service.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_content_type.h"

namespace readaloud {

ReadAloudPlaybackSession::ReadAloudPlaybackSession(
    content::WebContents* web_contents,
    ReadAloudService* service)
    : content::WebContentsObserver(web_contents), service_(service) {}

ReadAloudPlaybackSession::~ReadAloudPlaybackSession() {
  if (web_contents()) {
    if (auto* session = content::MediaSession::GetIfExists(web_contents())) {
      session->RemovePlayer(this, kPlayerId);
    }
  }
}

void ReadAloudPlaybackSession::NotifyPlaybackStarted() {
  is_playback_in_progress_ = true;
  is_paused_ = false;
  if (web_contents()) {
    if (auto* session = content::MediaSession::Get(web_contents())) {
      session->AddPlayer(this, kPlayerId);
    }
  }
}

void ReadAloudPlaybackSession::NotifyPlaybackPaused() {
  is_paused_ = true;
  if (web_contents()) {
    if (auto* session = content::MediaSession::GetIfExists(web_contents())) {
      session->OnPlayerPaused(this, kPlayerId);
    }
  }
}

void ReadAloudPlaybackSession::NotifyPlaybackStopped() {
  is_playback_in_progress_ = false;
  is_paused_ = true;
  if (web_contents()) {
    if (auto* session = content::MediaSession::GetIfExists(web_contents())) {
      session->RemovePlayer(this, kPlayerId);
    }
  }
}

void ReadAloudPlaybackSession::OnSuspend(int player_id,
                                         bool triggered_by_user) {
  if (player_id != kPlayerId) {
    return;
  }
  service_->OnSessionSuspended();
}

void ReadAloudPlaybackSession::OnResume(int player_id, bool triggered_by_user) {
  if (player_id != kPlayerId) {
    return;
  }
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

void ReadAloudPlaybackSession::OnRequestVisibility(
    int player_id,
    RequestVisibilityCallback callback) {
  std::move(callback).Run(false);
}

content::RenderFrameHost* ReadAloudPlaybackSession::render_frame_host() const {
  return web_contents() ? web_contents()->GetPrimaryMainFrame() : nullptr;
}

std::optional<media_session::MediaPosition>
ReadAloudPlaybackSession::GetPosition(int player_id) const {
  // TODO(b/524283800): Support media position.
  return std::nullopt;
}

bool ReadAloudPlaybackSession::IsPictureInPictureAvailable(
    int player_id) const {
  return false;
}

bool ReadAloudPlaybackSession::HasSufficientlyVisibleVideo(
    int player_id) const {
  return false;
}

bool ReadAloudPlaybackSession::HasAudio(int player_id) const {
  return true;
}

bool ReadAloudPlaybackSession::HasVideo(int player_id) const {
  return false;
}

bool ReadAloudPlaybackSession::IsPaused(int player_id) const {
  return is_paused_;
}

std::string ReadAloudPlaybackSession::GetAudioOutputSinkId(
    int player_id) const {
  return "";
}

bool ReadAloudPlaybackSession::SupportsAudioOutputDeviceSwitching(
    int player_id) const {
  return false;
}

media::MediaContentType ReadAloudPlaybackSession::GetMediaContentType() const {
  return media::MediaContentType::kPersistent;
}

}  // namespace readaloud
