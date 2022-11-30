// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SESSION_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SESSION_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class MediaEngagementService;

// Represents a session with regards to the Media Engagement. A session consists
// of a visit on a tab and all the tabs opened to the same origin originating
// from that tab or one of its children.
//
// Each tab will have a MediaEngagementContentsObserver that will hold a
// reference to a session. When all the references are released, the session
// will be destructed and record all the information. It guarantees that
// information are recorded only once.
class MediaEngagementSession : public base::RefCounted<MediaEngagementSession> {
 public:
  enum class RestoreType {
    kNotRestored = 0,
    kRestored,
  };

  MediaEngagementSession(MediaEngagementService* service,
                         const url::Origin& origin,
                         RestoreType restore_status,
                         ukm::SourceId ukm_source_id);

  MediaEngagementSession(const MediaEngagementSession&) = delete;
  MediaEngagementSession& operator=(const MediaEngagementSession&) = delete;

  // Returns whether the session's origin is same origin with |url|.
  bool IsSameOriginWith(const GURL& url) const;

  // Record that the session received a significant playback from a media
  // element.
  void RecordSignificantMediaElementPlayback();

  // Record that the session received a significant playback from an audio
  // context.
  void RecordSignificantAudioContextPlayback();

  // Record the length of an ignored media playback.
  void RecordShortPlaybackIgnored(int length_msec);

  // Register new audible/significant players. `significant_players` can't be
  // greater than `audible_players`.
  void RegisterAudiblePlayers(int32_t audible_players,
                              int32_t significant_players);

  // Whether any significant playback was recorded.
  bool WasSignificantPlaybackRecorded() const;

  bool significant_media_element_playback_recorded() const;
  bool significant_audio_context_playback_recorded() const;
  const url::Origin& origin() const;

 private:
  friend class base::RefCounted<MediaEngagementSession>;
  friend class MediaEngagementSessionTest;

  // Private because the class is RefCounted.
  ~MediaEngagementSession();

  // Records a significant playback that is either a media element or an audio
  // context playback.
  void RecordSignificantPlayback();

  // Record the score and change in score to UKM.
  void RecordUkmMetrics();

  // Returns whether the session has a pending playback that needs to be
  // committed into the database.
  bool HasPendingPlaybackToCommit() const;

  // Returns whether the session has data that needs to be committed into the
  // database.
  bool HasPendingDataToCommit() const;

  // Commits any pending data to website settings.
  void CommitPendingData();

  // Weak pointer because |this| has a lifetime shorter than it.
  raw_ptr<MediaEngagementService> service_;

  // Origin associated with the session.
  url::Origin origin_;

  // UKM source ID associated with the session. It will be used by all the
  // session clients.
  ukm::SourceId ukm_source_id_;

  // Delta counts are counts to be added to the score while total counts are
  // the sum of all the changes that happened during the session lifetime. The
  // total count is recorded as delta with regards to UKM.
  int32_t audible_players_delta_ = 0;
  int32_t significant_players_delta_ = 0;
  int32_t audible_players_total_ = 0;
  int32_t significant_players_total_ = 0;

  bool significant_media_element_playback_recorded_ = false;
  bool significant_audio_context_playback_recorded_ = false;
  struct PendingDataToCommit {
    bool visit = true;
    bool audio_context_playback = false;
    bool media_element_playback = false;
    bool players = false;
  };
  PendingDataToCommit pending_data_to_commit_;

  // The time the first significant playback occurred.
  base::Time first_significant_playback_time_;

  // Whether the session was restored.
  RestoreType restore_status_ = RestoreType::kNotRestored;

  // If the |is_high_| bit has changed since this object was created.
  bool high_score_changed_ = false;
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SESSION_H_
