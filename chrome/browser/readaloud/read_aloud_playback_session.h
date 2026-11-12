// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_READ_ALOUD_PLAYBACK_SESSION_H_
#define CHROME_BROWSER_READALOUD_READ_ALOUD_PLAYBACK_SESSION_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/media_session_player_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/media_position.h"

namespace content {
class MediaSession;
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace readaloud {

class ReadAloudService;

class ReadAloudPlaybackSession : public content::MediaSessionPlayerObserver,
                                 public content::WebContentsObserver {
 public:
  // Callback used to retrieve the `content::MediaSession` for a given
  // `WebContents`. Primarily used to inject mock sessions during testing.
  using MediaSessionGetter =
      base::RepeatingCallback<content::MediaSession*(content::WebContents*)>;

  ReadAloudPlaybackSession(content::WebContents* web_contents,
                           ReadAloudService* service,
                           MediaSessionGetter media_session_getter = {});
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
  //
  // `NotifyPlaybackStarted()` returns `false` if `MediaSession::AddPlayer()`
  // rejected the player (e.g., when system audio focus was denied).
  bool NotifyPlaybackStarted();
  void NotifyPlaybackPaused(bool end_of_media = false);
  void NotifyPlaybackStopped();
  void OnPlaybackProgressChanged(base::TimeDelta elapsed,
                                 base::TimeDelta duration);
  void OnPlaybackRateChanged(float rate);

  // Sets custom metadata for system media controls. `publisher` is mapped to
  // `source_title` to represent the publishing site rather than an artist.
  void SetCustomMetadata(const std::string& title,
                         const std::string& publisher);

  // content::MediaSessionPlayerObserver overrides:
  void OnSuspend(int player_id, bool triggered_by_user) override;
  void OnResume(int player_id, bool triggered_by_user) override;
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekTo(int player_id, base::TimeDelta seek_time) override;
  std::optional<media_session::MediaPosition> GetPosition(
      int player_id) const override;
  std::optional<media_session::MediaMetadata> GetMetadata(
      int player_id) const override;
  content::RenderFrameHost* render_frame_host() const override;
  bool HasAudio(int player_id) const override;
  bool IsPaused(int player_id) const override;

 private:
  // A suspend or resume initiated by `MediaSession` whose `Notify*()` call has
  // not happened yet.
  enum class SessionCommand { kNone, kSuspend, kResume };

  // Per-playback state used to compute and throttle `MediaPosition` updates for
  // the media notification progress bar / seekbar.
  struct PositionState {
    bool HasValidDuration() const { return reported_duration.is_positive(); }

    void ClearReported() {
      reported_duration = base::TimeDelta();
      last_duration_update_time = base::TimeTicks();
      last_reported_position.reset();
    }

    bool end_of_media = false;
    base::TimeDelta elapsed;
    base::TimeDelta reported_duration;
    base::TimeDelta latest_received_duration;
    base::TimeTicks last_duration_update_time;
    std::optional<media_session::MediaPosition> last_reported_position;
  };

  // Minimum interval between duration changes exposed to `MediaSession` once a
  // duration has been reported. Matches
  // `content::MediaSessionImpl::kDurationUpdateAllowanceIncreaseInterval` (20s)
  // so incremental TTS chunk duration refinements do not exhaust
  // `MaybeGuardDurationUpdate()`'s 3-update allowance and trigger its
  // livestream heuristic.
  // TODO(b/582792129): Link this constant directly with
  // `content::MediaSessionImpl::kDurationUpdateAllowanceIncreaseInterval` so
  // changes to the `MediaSessionImpl` throttle automatically apply here.
  static constexpr base::TimeDelta kMinDurationUpdateInterval =
      base::Seconds(20);

  // Minimum deviation between a reported `elapsed` time and the position
  // extrapolated from `position_.last_reported_position` required to push an
  // unprompted position update during continuous playback.
  static constexpr base::TimeDelta kPositionDriftThreshold = base::Seconds(1);

  // Consumes any pending MediaSession command and returns whether it matched
  // `command`.
  bool ConsumePendingSessionCommand(SessionCommand command);

  // Updates `position_.reported_duration` from
  // `position_.latest_received_duration` when permitted by the duration update
  // guard interval. Returns whether `position_.reported_duration` changed.
  bool MaybeUpdateReportedDuration();

  // Triggers `MediaSession` to re-query `GetPosition()` when Read Aloud is
  // registered in the session.
  void RefreshMediaSessionPosition();

  content::MediaSession* GetOrCreateMediaSession();
  content::MediaSession* GetMediaSessionIfExists();

  const raw_ptr<ReadAloudService> service_;
  MediaSessionGetter media_session_getter_;
  static constexpr int kPlayerId = 0;

  bool is_paused_ = true;
  bool is_registered_ = false;

  std::optional<media_session::MediaMetadata> custom_metadata_;
  SessionCommand pending_session_command_ = SessionCommand::kNone;

  double playback_rate_ = 1.0;
  PositionState position_;
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_READ_ALOUD_PLAYBACK_SESSION_H_
