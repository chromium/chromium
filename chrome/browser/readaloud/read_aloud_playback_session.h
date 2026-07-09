// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_READ_ALOUD_PLAYBACK_SESSION_H_
#define CHROME_BROWSER_READALOUD_READ_ALOUD_PLAYBACK_SESSION_H_

#include <optional>
#include <string>

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

  // Called by ReadAloudService to synchronize internal state.
  void NotifyPlaybackStarted();
  void NotifyPlaybackPaused();
  void NotifyPlaybackStopped();

  bool is_paused() const { return is_paused_; }
  bool is_playback_in_progress() const { return is_playback_in_progress_; }

  // content::MediaSessionPlayerObserver overrides:
  void OnSuspend(int player_id, bool triggered_by_user) override;
  void OnResume(int player_id, bool triggered_by_user) override;
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekTo(int player_id, base::TimeDelta seek_time) override;
  void OnSetVolumeMultiplier(int player_id, double volume_multiplier) override {
  }
  void OnEnterPictureInPicture(
      int player_id,
      const std::optional<gfx::Size>& min_size) override {}
  void OnSetAudioSinkId(int player_id,
                        const std::string& raw_device_id) override {}
  void OnSetMute(int player_id, bool mute) override {}
  void OnRequestMediaRemoting(int player_id) override {}
  void OnRequestVisibility(int player_id,
                           RequestVisibilityCallback callback) override;

  content::RenderFrameHost* render_frame_host() const override;
  std::optional<media_session::MediaPosition> GetPosition(
      int player_id) const override;

  bool IsPictureInPictureAvailable(int player_id) const override;
  bool HasSufficientlyVisibleVideo(int player_id) const override;
  bool HasAudio(int player_id) const override;
  bool HasVideo(int player_id) const override;
  bool IsPaused(int player_id) const override;
  std::string GetAudioOutputSinkId(int player_id) const override;
  bool SupportsAudioOutputDeviceSwitching(int player_id) const override;
  media::MediaContentType GetMediaContentType() const override;
  void OnAutoPictureInPictureInfoChanged(
      int player_id,
      const media::PictureInPictureEventsInfo::AutoPipInfo& info) override {}

 private:
  const raw_ptr<ReadAloudService> service_;
  static constexpr int kPlayerId = 0;

  bool is_paused_ = true;
  bool is_playback_in_progress_ = false;

  base::WeakPtrFactory<ReadAloudPlaybackSession> weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_READ_ALOUD_PLAYBACK_SESSION_H_
