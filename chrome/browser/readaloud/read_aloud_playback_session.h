// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_READ_ALOUD_PLAYBACK_SESSION_H_
#define CHROME_BROWSER_READALOUD_READ_ALOUD_PLAYBACK_SESSION_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/media_session_player_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/media_position.h"

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace readaloud {

class ReadAloudService;

class ReadAloudPlaybackSession : public content::MediaSessionPlayerObserver,
                                 public content::WebContentsObserver {
 public:
  ReadAloudPlaybackSession(content::WebContents* web_contents,
                           ReadAloudService* service);
  ReadAloudPlaybackSession(const ReadAloudPlaybackSession&) = delete;
  ReadAloudPlaybackSession& operator=(const ReadAloudPlaybackSession&) = delete;
  ~ReadAloudPlaybackSession() override;

  // Called by `ReadAloudService` to synchronize internal state.
  //
  // If `MediaSession` itself suspended or resumed Read Aloud (`OnSuspend()` or
  // `OnResume()`), the matching `NotifyPlaybackPaused()` or
  // `NotifyPlaybackStarted()` only updates local state. `MediaSession` has
  // already updated its own state, and calling back into it can change its
  // player list while it is iterating over it (b/572721812).
  void NotifyPlaybackStarted();
  void NotifyPlaybackPaused();
  void NotifyPlaybackStopped();

  bool is_paused() const { return is_paused_; }

  // content::MediaSessionPlayerObserver overrides:
  void OnSuspend(int player_id, bool triggered_by_user) override;
  void OnResume(int player_id, bool triggered_by_user) override;
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekTo(int player_id, base::TimeDelta seek_time) override;

  content::RenderFrameHost* render_frame_host() const override;
  bool HasAudio(int player_id) const override;
  bool IsPaused(int player_id) const override;

 private:
  // A suspend or resume initiated by `MediaSession` whose `Notify*()` call has
  // not happened yet.
  enum class SessionCommand { kNone, kSuspend, kResume };

  // Consumes any pending MediaSession command and returns whether it matched
  // `command`.
  bool ConsumePendingSessionCommand(SessionCommand command);

  const raw_ptr<ReadAloudService> service_;
  static constexpr int kPlayerId = 0;

  bool is_paused_ = true;

  SessionCommand pending_session_command_ = SessionCommand::kNone;

  base::WeakPtrFactory<ReadAloudPlaybackSession> weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_READ_ALOUD_PLAYBACK_SESSION_H_
