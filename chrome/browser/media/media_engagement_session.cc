// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_session.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/media/media_engagement_preloaded_list.h"
#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "media/base/media_switches.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

// This is used for histograms. Do not re-order or change values.
enum class SessionStatus {
  kCreated = 0,
  kSignificantPlayback = 1,
  // Leave at the end.
  kSize,
};

void RecordSessionStatus(SessionStatus status) {
  static const char kSessionStatus[] = "Media.Engagement.Session";
  UMA_HISTOGRAM_ENUMERATION(kSessionStatus, status, SessionStatus::kSize);
}

void RecordRestoredSessionStatus(SessionStatus status) {
  static const char kSessionRestoredStatus[] =
      "Media.Engagement.Session.Restored";
  UMA_HISTOGRAM_ENUMERATION(kSessionRestoredStatus, status,
                            SessionStatus::kSize);
}

}  // anonymous namespace

MediaEngagementSession::MediaEngagementSession(MediaEngagementService* service,
                                               const url::Origin& origin,
                                               RestoreType restore_status)
    : service_(service), origin_(origin), restore_status_(restore_status) {
  if (restore_status_ == RestoreType::kRestored)
    pending_data_to_commit_.visit = false;
}

bool MediaEngagementSession::IsSameOriginWith(const url::Origin& origin) const {
  return origin_.IsSameOriginWith(origin);
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
  ukm::UkmRecorder* ukm_recorder = GetUkmRecorder();
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

  if (HasPendingDataToCommit()) {
    CommitPendingData();
  } else if ((restore_status_ == RestoreType::kRestored) &&
             !WasSignificantPlaybackRecorded()) {
    RecordStatusHistograms();
  }

  RecordUkmMetrics();
}

ukm::UkmRecorder* MediaEngagementSession::GetUkmRecorder() {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return nullptr;

  if (ukm_source_id_ == ukm::kInvalidSourceId) {
    ukm_source_id_ = ukm_recorder->GetNewSourceID();
    ukm_recorder->UpdateSourceURL(ukm_source_id_, origin_.GetURL());
  }

  return ukm_recorder;
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
  ukm::UkmRecorder* ukm_recorder = GetUkmRecorder();
  if (!ukm_recorder)
    return;

  bool is_preloaded = false;
  if (base::FeatureList::IsEnabled(media::kPreloadMediaEngagementData)) {
    is_preloaded =
        MediaEngagementPreloadedList::GetInstance()->CheckOriginIsPresent(
            origin_);
  }

  MediaEngagementScore score =
      service_->CreateEngagementScore(origin_.GetURL());
  ukm::builders::Media_Engagement_SessionFinished(ukm_source_id_)
      .SetPlaybacks_AudioContextTotal(score.audio_context_playbacks())
      .SetPlaybacks_MediaElementTotal(score.media_element_playbacks())
      .SetPlaybacks_Total(score.media_playbacks())
      .SetVisits_Total(score.visits())
      .SetEngagement_Score(round(score.actual_score() * 100))
      .SetPlaybacks_Delta(significant_media_element_playback_recorded_)
      .SetEngagement_IsHigh(score.high_score())
      .SetEngagement_IsHigh_Changed(high_score_changed_)
      .SetEngagement_IsHigh_Changes(score.high_score_changes())
      .SetEngagement_IsPreloaded(is_preloaded)
      .SetPlayer_Audible_Delta(audible_players_total_)
      .SetPlayer_Audible_Total(score.audible_playbacks())
      .SetPlayer_Significant_Delta(significant_players_total_)
      .SetPlayer_Significant_Total(score.significant_playbacks())
      .SetPlaybacks_SecondsSinceLast(time_since_playback_for_ukm_.InSeconds())
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

void MediaEngagementSession::RecordStatusHistograms() const {
  DCHECK(HasPendingDataToCommit() ||
         (restore_status_ == RestoreType::kRestored));

  RecordSessionStatus(SessionStatus::kCreated);
  if (HasPendingPlaybackToCommit())
    RecordSessionStatus(SessionStatus::kSignificantPlayback);

  if (restore_status_ == RestoreType::kRestored) {
    RecordRestoredSessionStatus(SessionStatus::kCreated);
    if (HasPendingPlaybackToCommit())
      RecordRestoredSessionStatus(SessionStatus::kSignificantPlayback);
  }
}

void MediaEngagementSession::CommitPendingData() {
  DCHECK(HasPendingDataToCommit());

  RecordStatusHistograms();

  MediaEngagementScore score =
      service_->CreateEngagementScore(origin_.GetURL());
  bool previous_high_value = score.high_score();

  if (pending_data_to_commit_.visit)
    score.IncrementVisits();

  if (WasSignificantPlaybackRecorded() && HasPendingPlaybackToCommit()) {
    const base::Time old_time = score.last_media_playback_time();

    score.IncrementMediaPlaybacks();

    if (pending_data_to_commit_.audio_context_playback)
      score.IncrementAudioContextPlaybacks();

    if (pending_data_to_commit_.media_element_playback)
      score.IncrementMediaElementPlaybacks();

    // Use the stored significant playback time.
    score.set_last_media_playback_time(first_significant_playback_time_);

    // This code should be reached once and |time_since_playback_for_ukm_| can't
    // be set.
    DCHECK(time_since_playback_for_ukm_.is_zero());

    if (!old_time.is_null()) {
      // Calculate the time since the last playback and the first significant
      // playback this visit. If there is no last playback time then we will
      // record 0.
      time_since_playback_for_ukm_ =
          score.last_media_playback_time() - old_time;
    }
  }

  if (pending_data_to_commit_.players) {
    score.IncrementAudiblePlaybacks(audible_players_delta_);
    score.IncrementSignificantPlaybacks(significant_players_delta_);

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
