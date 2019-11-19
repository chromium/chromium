// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_

#include "base/timer/timer.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class Clock;
}  // namespace base

namespace gfx {
class Size;
}  // namespace gfx

class MediaEngagementContentsObserverTest;
class MediaEngagementService;
class MediaEngagementSession;

class MediaEngagementContentsObserver : public content::WebContentsObserver {
 public:
  ~MediaEngagementContentsObserver() override;

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void MediaStartedPlaying(
      const MediaPlayerInfo& media_player_info,
      const content::MediaPlayerId& media_player_id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& media_player_info,
      const content::MediaPlayerId& media_player_id,
      WebContentsObserver::MediaStoppedReason reason) override;
  void DidUpdateAudioMutingState(bool muted) override;
  void MediaMutedStatusChanged(const content::MediaPlayerId& id,
                               bool muted) override;
  void MediaResized(const gfx::Size& size,
                    const content::MediaPlayerId& id) override;
  void AudioContextPlaybackStarted(
      const AudioContextId& audio_context_id) override;
  void AudioContextPlaybackStopped(
      const AudioContextId& audio_context_id) override;

  static const gfx::Size kSignificantSize;
  static const char* const kHistogramScoreAtPlaybackName;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaEngagementContentsObserverTest,
                           RecordInsignificantReason);
  FRIEND_TEST_ALL_PREFIXES(MediaEngagementContentsObserverTest,
                           RecordInsignificantReason_NotAdded_AfterFirstTime);
  // Only MediaEngagementService can create a MediaEngagementContentsObserver.
  friend MediaEngagementService;
  friend MediaEngagementContentsObserverTest;
  friend class MediaEngagementBrowserTest;

  MediaEngagementContentsObserver(content::WebContents* web_contents,
                                  MediaEngagementService* service);

  // This is the maximum playback time for media to be considered 'short'.
  static const base::TimeDelta kMaxShortPlaybackTime;

  // This enum is used to record a histogram and should not be renumbered.
  enum class InsignificantPlaybackReason {
    // The frame size of the video is too small.
    kFrameSizeTooSmall = 0,

    // The player was muted.
    kAudioMuted,

    // The media is paused.
    kMediaPaused,

    // No audio track was present on the media.
    kNoAudioTrack,

    // This is always recorded to the histogram so we can see the number
    // of events that occured.
    kCount,

    // Add new items before this one, always keep this one at the end.
    kReasonMax,
  };

  enum class InsignificantHistogram {
    // The player isn't currently significant and can't be because it
    // doesn't meet all the criteria (first time only).
    kPlayerNotAddedFirstTime = 0,

    // The player isn't currently significant and can't be because it
    // doesn't meet all the critera (after the first time).
    kPlayerNotAddedAfterFirstTime,

    // The player was significant but no longer meets the criteria.
    kPlayerRemoved,
  };

  void OnSignificantMediaPlaybackTimeForPlayer(
      const content::MediaPlayerId& id);
  void OnSignificantMediaPlaybackTimeForPage();
  void OnSignificantAudioContextPlaybackTimeForPage();

  void UpdatePlayerTimer(const content::MediaPlayerId&);
  void UpdatePageTimer();
  void UpdateAudioContextTimer();

  bool AreConditionsMet() const;
  bool AreAudioContextConditionsMet() const;

  void SetTaskRunnerForTest(scoped_refptr<base::SequencedTaskRunner>);

  // |this| is owned by |service_|.
  MediaEngagementService* service_;

  // Timer that will fire when the playback time reaches the minimum for
  // significant media playback.
  base::OneShotTimer playback_timer_;

  // Set of active players that can produce a significant playback. In other
  // words, whether this set is empty can be used to know if there is a
  // significant playback.
  std::set<content::MediaPlayerId> significant_players_;

  // Timer that will fire when the playback time of any audio context reaches
  // the minimum for significant media playback.
  base::OneShotTimer audio_context_timer_;

  // Set of active audio contexts that can produce a significant playback.
  std::set<AudioContextId> audio_context_players_;

  // Measures playback time for a player.
  class PlaybackTimer {
   public:
    explicit PlaybackTimer(base::Clock*);

    void Start();
    void Stop();
    bool IsRunning() const;
    base::TimeDelta Elapsed() const;
    void Reset();

    DISALLOW_COPY_AND_ASSIGN(PlaybackTimer);

   private:
    // The clock is owned by |service_| which already owns |this|.
    base::Clock* clock_;

    base::Optional<base::Time> start_time_;
    base::TimeDelta recorded_time_;
  };

  // A structure containing all the information we have about a player's state.
  struct PlayerState {
    explicit PlayerState(base::Clock*);
    ~PlayerState();
    PlayerState(PlayerState&&);

    base::Optional<bool> muted;
    base::Optional<bool> playing;           // Currently playing.
    base::Optional<bool> significant_size;  // The video track has at least
                                            // a certain frame size.
    base::Optional<bool> has_audio;         // The media has an audio track.
    base::Optional<bool> has_video;         // The media has a video track.

    // The engagement score of the origin at playback has been recorded
    // to a histogram.
    bool score_recorded = false;

    // The reasons why the player was not significant have been recorded
    // to a histogram.
    bool reasons_recorded = false;

    bool reached_end_of_stream = false;
    std::unique_ptr<PlaybackTimer> playback_timer;

    DISALLOW_COPY_AND_ASSIGN(PlayerState);
  };
  std::map<content::MediaPlayerId, PlayerState> player_states_;
  PlayerState& GetPlayerState(const content::MediaPlayerId& id);
  void ClearPlayerStates();

  // Inserts/removes players from significant_players_ based on whether
  // they are considered significant by GetInsignificantPlayerReason.
  void MaybeInsertRemoveSignificantPlayer(const content::MediaPlayerId& id);

  // Returns a vector containing InsignificantPlaybackReason's why a player
  // would not be considered significant.
  std::vector<MediaEngagementContentsObserver::InsignificantPlaybackReason>
  GetInsignificantPlayerReasons(const PlayerState& state);

  // Records why a player is not significant to a historgram.
  void RecordInsignificantReasons(
      std::vector<InsignificantPlaybackReason> reasons,
      const PlayerState& state,
      InsignificantHistogram histogram);

  // Returns whether we have recieved all the state information about a
  // player in order to be able to make a decision about it.
  bool IsPlayerStateComplete(const PlayerState& state);

  // Returns whether the player is considered to be significant and record
  // any reasons why not to a histogram.
  bool IsSignificantPlayerAndRecord(
      const content::MediaPlayerId& id,
      MediaEngagementContentsObserver::InsignificantHistogram histogram);

  static const char* const kHistogramSignificantNotAddedAfterFirstTimeName;
  static const char* const kHistogramSignificantNotAddedFirstTimeName;
  static const char* const kHistogramSignificantRemovedName;
  static const int kMaxInsignificantPlaybackReason;

  static const base::TimeDelta kSignificantMediaPlaybackTime;

  // Records the engagement score for the current origin to a histogram so we
  // can identify whether the playback would have been blocked.
  void RecordEngagementScoreToHistogramAtPlayback(
      const content::MediaPlayerId& id);

  // Clears out players that are ignored because they are too short and register
  // the result as significant/audible players with the `session_`.
  void RegisterAudiblePlayersWithSession();

  // Returns the opener of the current WebContents. Null if there is none.
  content::WebContents* GetOpener() const;

  // Find the appropriate media engagement session if any or create a new one to
  // be used. Will return nullptr if no session should be used.
  scoped_refptr<MediaEngagementSession> GetOrCreateSession(
      content::NavigationHandle* navigation_handle,
      content::WebContents* opener) const;

  // Stores the ids of the players that were audible. The boolean will be true
  // if the player was significant.
  using AudiblePlayerRow = std::pair<bool, std::unique_ptr<base::OneShotTimer>>;
  std::map<content::MediaPlayerId, AudiblePlayerRow> audible_players_;

  // The task runner to use when creating timers. It is used only for testing.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The MediaEngagementSession used by this MediaEngagementContentsObserver. It
  // may be shared by other instances if they are part of the same session. It
  // willl be null if it is not part of a session.
  scoped_refptr<MediaEngagementSession> session_;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementContentsObserver);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_CONTENTS_OBSERVER_H_
