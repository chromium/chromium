// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_playback_session.h"

#include <utility>

#include "chrome/browser/readaloud/read_aloud_service.h"
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
  if (ConsumePendingSessionCommand(SessionCommand::kResume)) {
    // `MediaSession` initiated the resume and already holds audio focus.
    return true;
  }
  if (content::MediaSession* session = GetOrCreateMediaSession()) {
    if (!session->AddPlayer(this, kPlayerId)) {
      is_paused_ = true;
      return false;
    }
  }
  return true;
}

void ReadAloudPlaybackSession::NotifyPlaybackPaused() {
  is_paused_ = true;
  if (ConsumePendingSessionCommand(SessionCommand::kSuspend)) {
    // `MediaSession` initiated the suspend and already updated its own state.
    return;
  }
  if (content::MediaSession* session = GetMediaSessionIfExists()) {
    session->OnPlayerPaused(this, kPlayerId);
  }
}

void ReadAloudPlaybackSession::NotifyPlaybackStopped() {
  is_paused_ = true;
  pending_session_command_ = SessionCommand::kNone;
  if (content::MediaSession* session = GetMediaSessionIfExists()) {
    session->RemovePlayer(this, kPlayerId);
  }
}

bool ReadAloudPlaybackSession::ConsumePendingSessionCommand(
    SessionCommand command) {
  return std::exchange(pending_session_command_, SessionCommand::kNone) ==
         command;
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
