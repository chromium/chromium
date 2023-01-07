// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_session.h"

#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "media/base/media_switches.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

MediaEngagementSession::MediaEngagementSession(MediaEngagementService* service,
                                               const url::Origin& origin,
                                               RestoreType restore_status,
                                               ukm::SourceId ukm_source_id)
    : service_(service),
      origin_(origin),
      ukm_source_id_(ukm_source_id),
      restore_status_(restore_status) {
  if (restore_status_ == RestoreType::kRestored)
    pending_data_to_commit_.visit = false;
}

bool MediaEngagementSession::IsSameOriginWith(const GURL& url) const {
  return origin_.IsSameOriginWith(url);
}

void MediaEngagementSession::RecordSignificantMediaElementPlayback() {
  DCHECK(!significant_media_element_playback_recorded_);

  significant_media_element_playback_recorded_ = true;
  pending_data_to_commit_.media_element_playback = true;

  RecordSignificantPlayback();
}

void MediaEngagementSession::RecordSignificantAudioContextPlayback() {
  DCHECK(!significant_audio_context_playback_recorded_);

  significant_audio_context_playback_recorded_ = true;
  pending_data_to_commit_.audio_context_playback = true;

  RecordSignificantPlayback();
}

void MediaEngagementSession::RecordShortPlaybackIgnored(int length_msec) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  ukm::builders::Media_Engagement_ShortPlaybackIgnored(ukm_source_id_)
      .SetLength(length_msec)
      .Record(ukm_recorder);
}

void MediaEngagementSession::RegisterAudiblePlayers(
    int32_t audible_players,
    int32_t significant_players) {
  DCHECK_GE(audible_players, significant_players);

  if (!audible_players && !significant_players)
    return;

  pending_data_to_commit_.players = true;
  audible_players_delta_ += audible_players;
  significant_players_delta_ += significant_players;
}

bool MediaEngagementSession::WasSignificantPlaybackRecorded() const {
  return significant_media_element_playback_recorded_ ||
         significant_audio_context_playback_recorded_;
}

bool MediaEngagementSession::significant_media_element_playback_recorded()
    const {
  return significant_media_element_playback_recorded_;
}

bool MediaEngagementSession::significant_audio_context_playback_recorded()
    const {
  return significant_audio_context_playback_recorded_;
}

const url::Origin& MediaEngagementSession::origin() const {
  return origin_;
}

MediaEngagementSession::~MediaEngagementSession() {
  // The destructor is called when all the tabs associated te the MEI session
  // are closed. Metrics and data related to "visits" need to be recorded now.

  if (HasPendingDataToCommit())
    CommitPendingData();

  RecordUkmMetrics();
}

void MediaEngagementSession::RecordSignificantPlayback() {
  DCHECK(WasSignificantPlaybackRecorded());

  // If this was the first time we recorded significant playback then we should
  // record the playback time.
  if (first_significant_playback_time_.is_null())
    first_significant_playback_time_ = service_->clock()->Now();

  // When a session was restored, visits are only recorded when there was a
  // playback. Add back the visit now as this code can only be executed once
  // per session.
  if (restore_status_ == RestoreType::kRestored)
    pending_data_to_commit_.visit = true;
}

void MediaEngagementSession::RecordUkmMetrics() {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  MediaEngagementScore score = service_->CreateEngagementScore(origin_);
  ukm::builders::Media_Engagement_SessionFinished(ukm_source_id_)
      .SetPlaybacks_Total(score.media_playbacks())
      .SetVisits_Total(score.visits())
      .SetEngagement_Score(round(score.actual_score() * 100))
      .SetEngagement_IsHigh(score.high_score())
      .SetPlayer_Audible_Delta(audible_players_total_)
      .SetPlayer_Significant_Delta(significant_players_total_)
      .Record(ukm_recorder);
}

bool MediaEngagementSession::HasPendingPlaybackToCommit() const {
  return pending_data_to_commit_.audio_context_playback ||
         pending_data_to_commit_.media_element_playback;
}

bool MediaEngagementSession::HasPendingDataToCommit() const {
  return pending_data_to_commit_.visit || pending_data_to_commit_.players ||
         HasPendingPlaybackToCommit();
}

void MediaEngagementSession::CommitPendingData() {
  DCHECK(HasPendingDataToCommit());

  MediaEngagementScore score = service_->CreateEngagementScore(origin_);
  bool previous_high_value = score.high_score();

  if (pending_data_to_commit_.visit)
    score.IncrementVisits();

  if (WasSignificantPlaybackRecorded() && HasPendingPlaybackToCommit()) {
    score.IncrementMediaPlaybacks();

    // Use the stored significant playback time.
    score.set_last_media_playback_time(first_significant_playback_time_);
  }

  if (pending_data_to_commit_.players) {
    audible_players_total_ += audible_players_delta_;
    significant_players_total_ += significant_players_delta_;

    audible_players_delta_ = 0;
    significant_players_delta_ = 0;
  }

  score.Commit();

  // If the high state has changed store that in a bool.
  high_score_changed_ = previous_high_value != score.high_score();

  pending_data_to_commit_.visit = pending_data_to_commit_.players =
      pending_data_to_commit_.audio_context_playback =
          pending_data_to_commit_.media_element_playback = false;
}
