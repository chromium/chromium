// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_metrics_recorder.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_histogram_names.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace {

const char* GetNameSuffixBySessionDuration(int session_duration) {
  if (session_duration <= 10) {
    return focus_mode_histogram_names::kShortSuffix;
  } else if (session_duration >= 30) {
    return focus_mode_histogram_names::kLongSuffix;
  }
  return focus_mode_histogram_names::kMediumSuffix;
}

void RecordInitialDurationHistogram(base::TimeDelta session_duration) {
  base::UmaHistogramCustomCounts(
      /*name=*/focus_mode_histogram_names::
          kInitialDurationOnSessionStartsHistogramName,
      /*sample=*/session_duration.InMinutes(),
      /*min=*/focus_mode_util::kMinimumDuration.InMinutes(),
      /*max=*/focus_mode_util::kMaximumDuration.InMinutes(), /*buckets=*/50);
}

void RecordStartSessionSourceHistogram(
    focus_mode_histogram_names::ToggleSource source) {
  switch (source) {
    case focus_mode_histogram_names::ToggleSource::kFocusPanel:
      base::UmaHistogramEnumeration(
          /*name=*/focus_mode_histogram_names::kStartSessionSourceHistogramName,
          /*sample=*/focus_mode_histogram_names::StartSessionSource::
              kFocusPanel);
      break;
    case focus_mode_histogram_names::ToggleSource::kFeaturePod:
      base::UmaHistogramEnumeration(
          /*name=*/focus_mode_histogram_names::kStartSessionSourceHistogramName,
          /*sample=*/focus_mode_histogram_names::StartSessionSource::
              kFeaturePod);
      break;
    case ash::focus_mode_histogram_names::ToggleSource::kContextualPanel:
      NOTREACHED();
  }
}

void RecordTasksSelectedHistogram(const int tasks_selected_count) {
  base::UmaHistogramCounts100(
      focus_mode_histogram_names::kTasksSelectedHistogramName,
      tasks_selected_count);
}

void RecordTasksCompletedHistogram(const int tasks_completed_count) {
  base::UmaHistogramCounts100(
      focus_mode_histogram_names::kTasksCompletedHistogramName,
      tasks_completed_count);
}

void RecordPlaylistsPlayedHistogram(const int playlists_played_count) {
  base::UmaHistogramCounts100(
      focus_mode_histogram_names::kCountPlaylistsPlayedDuringSession,
      playlists_played_count);
}

void RecordDNDStateOnFocusEndHistogram(
    bool has_user_interactions_on_dnd_in_focus_session) {
  auto* message_center = message_center::MessageCenter::Get();
  const bool is_dnd_on = message_center->IsQuietMode();

  focus_mode_histogram_names::DNDStateOnFocusEndType type;
  if (is_dnd_on && message_center->GetLastQuietModeChangeSourceType() ==
                       message_center::QuietModeSourceType::kFocusMode) {
    type = focus_mode_histogram_names::DNDStateOnFocusEndType::kFocusModeOn;
  } else if (has_user_interactions_on_dnd_in_focus_session) {
    type = is_dnd_on
               ? focus_mode_histogram_names::DNDStateOnFocusEndType::kTurnedOn
               : focus_mode_histogram_names::DNDStateOnFocusEndType::kTurnedOff;
  } else {
    type =
        is_dnd_on
            ? focus_mode_histogram_names::DNDStateOnFocusEndType::kAlreadyOn
            : focus_mode_histogram_names::DNDStateOnFocusEndType::kAlreadyOff;
  }
  base::UmaHistogramEnumeration(
      /*name=*/focus_mode_histogram_names::kDNDStateOnFocusEndHistogramName,
      /*sample=*/type);
}

void RecordTimeAddedOnSessionEndHistogram(int initial_session_duration,
                                          int current_session_duration) {
  std::string histogram_name(
      focus_mode_histogram_names::kTimeAddedOnSessionEndPrefix);
  histogram_name.append(
      GetNameSuffixBySessionDuration(initial_session_duration));

  base::UmaHistogramCustomCounts(
      /*name=*/histogram_name,
      /*sample=*/current_session_duration - initial_session_duration,
      /*min=*/0,
      /*exclusive_max=*/focus_mode_util::kMaximumDuration.InMinutes(),
      /*buckets=*/50);
}

void RecordPercentCompletedHistogram(double progress,
                                     int final_session_duration) {
  std::string histogram_name(
      focus_mode_histogram_names::kPercentCompletedPrefix);
  histogram_name.append(GetNameSuffixBySessionDuration(final_session_duration));

  base::UmaHistogramPercentage(
      /*name=*/histogram_name,
      /*percent=*/(progress * 100));
}

void RecordSessionDurationHistogram(const int time_elapsed) {
  base::UmaHistogramCustomCounts(
      /*name=*/focus_mode_histogram_names::kSessionDurationHistogramName,
      /*sample=*/time_elapsed,
      /*min=*/0,
      /*exclusive_max=*/focus_mode_util::kMaximumDuration.InMinutes(),
      /*buckets=*/50);
}

// Return true if the current selected task is the task selected in the
// previous focus session.
bool IsPreviouslySelectedTask(const PrefService* active_user_prefs,
                              const TaskId& current_selected_task_id) {
  const auto& selected_task_dict =
      active_user_prefs->GetDict(prefs::kFocusModeSelectedTask);

  if (selected_task_dict.empty()) {
    return false;
  }

  const auto* previously_selected_task_id =
      selected_task_dict.FindString(focus_mode_util::kTaskIdKey);
  const auto* previously_selected_task_list_id =
      selected_task_dict.FindString(focus_mode_util::kTaskListIdKey);

  TaskId previous_id = {
      .list_id = previously_selected_task_list_id
                     ? *previously_selected_task_list_id
                     : "",
      .id = previously_selected_task_id ? *previously_selected_task_id : ""};

  return current_selected_task_id == previous_id;
}

void RecordStartedWithTaskHistogram(const TaskId& selected_task_id) {
  if (selected_task_id.empty()) {
    base::UmaHistogramEnumeration(
        /*name=*/focus_mode_histogram_names::
            kStartedWithTaskStatekHistogramName,
        /*sample=*/focus_mode_histogram_names::StartedWithTaskState::kNoTask);
    return;
  }

  PrefService* active_user_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!active_user_prefs) {
    // Can be null in tests.
    return;
  }

  base::UmaHistogramEnumeration(
      /*name=*/focus_mode_histogram_names::kStartedWithTaskStatekHistogramName,
      /*sample=*/IsPreviouslySelectedTask(active_user_prefs, selected_task_id)
          ? focus_mode_histogram_names::StartedWithTaskState::
                kPreviouslySelectedTask
          : focus_mode_histogram_names::StartedWithTaskState::
                kNewlySelectedTask);
}

void RecordSoundsPlayedDuringSessionHistogram(bool has_selected_soundscapes,
                                              bool has_selected_youtube_music) {
  focus_mode_histogram_names::PlaylistTypesSelectedDuringFocusSessionType type =
      focus_mode_histogram_names::PlaylistTypesSelectedDuringFocusSessionType::
          kNone;

  if (has_selected_soundscapes && has_selected_youtube_music) {
    type = focus_mode_histogram_names::
        PlaylistTypesSelectedDuringFocusSessionType::
            kYouTubeMusicAndSoundscapes;
  } else if (has_selected_soundscapes) {
    type = focus_mode_histogram_names::
        PlaylistTypesSelectedDuringFocusSessionType::kSoundscapes;
  } else if (has_selected_youtube_music) {
    type = focus_mode_histogram_names::
        PlaylistTypesSelectedDuringFocusSessionType::kYouTubeMusic;
  }

  base::UmaHistogramEnumeration(
      /*name=*/focus_mode_histogram_names::kPlaylistTypesSelectedDuringSession,
      /*sample=*/type);
}

void RecordExistingMediaPlayingOnStartHistogram() {
  const auto system_media_session_info =
      FocusModeController::Get()->GetSystemMediaSessionInfo();
  base::UmaHistogramBoolean(
      /*name=*/focus_mode_histogram_names::
          kStartedWithExistingMediaPlayingHistogramName,
      /*sample=*/system_media_session_info &&
          system_media_session_info->playback_state ==
              media_session::mojom::MediaPlaybackState::kPlaying);
}

void RecordPausedEventCountHistogram() {
  base::UmaHistogramCounts100(
      /*name=*/focus_mode_histogram_names::kMusicPausedEventsCount,
      /*sample=*/FocusModeController::Get()
          ->focus_mode_sounds_controller()
          ->paused_event_count());
}

}  // namespace

FocusModeMetricsRecorder::FocusModeMetricsRecorder(
    const base::TimeDelta& initial_session_duration)
    : initial_session_duration_(initial_session_duration) {
  message_center::MessageCenter::Get()->AddObserver(this);
}

FocusModeMetricsRecorder::~FocusModeMetricsRecorder() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
}

void FocusModeMetricsRecorder::IncrementTasksSelectedCount() {
  tasks_selected_count_++;
}

void FocusModeMetricsRecorder::IncrementTasksCompletedCount() {
  tasks_completed_count_++;
}

void FocusModeMetricsRecorder::OnQuietModeChanged(bool in_quiet_mode) {
  // Only set the value if it's in a focus session and not triggered by
  // Focus Mode.
  if (FocusModeController::Get()->in_focus_session() &&
      message_center::MessageCenter::Get()
              ->GetLastQuietModeChangeSourceType() !=
          message_center::QuietModeSourceType::kFocusMode) {
    has_user_interactions_on_dnd_in_focus_session_ = true;
  }
}

void FocusModeMetricsRecorder::SetHasSelectedSoundType(
    const focus_mode_util::SelectedPlaylist& selected_playlist) {
  if (selected_playlist.empty()) {
    return;
  }

  switch (selected_playlist.type) {
    case focus_mode_util::SoundType::kSoundscape:
      has_selected_soundscapes_ = true;
      break;
    case focus_mode_util::SoundType::kYouTubeMusic:
      has_selected_youtube_music_ = true;
      break;
    case focus_mode_util::SoundType::kNone:
      NOTREACHED();
  }

  playlists_played_count_++;
}

void FocusModeMetricsRecorder::RecordHistogramsOnStart(
    focus_mode_histogram_names::ToggleSource source,
    const TaskId& selected_task_id) {
  RecordInitialDurationHistogram(
      /*session_duration=*/initial_session_duration_);
  RecordStartSessionSourceHistogram(source);
  RecordStartedWithTaskHistogram(selected_task_id);
  RecordExistingMediaPlayingOnStartHistogram();
}

void FocusModeMetricsRecorder::RecordHistogramsOnEnd() {
  RecordTasksSelectedHistogram(tasks_selected_count_);
  RecordTasksCompletedHistogram(tasks_completed_count_);
  RecordPlaylistsPlayedHistogram(playlists_played_count_);
  RecordDNDHistogram();

  auto session_snapshot =
      FocusModeController::Get()->GetSnapshot(base::Time::Now());
  RecordTimeAddedOnSessionEndHistogram(
      initial_session_duration_.InMinutes(),
      session_snapshot.session_duration.InMinutes());
  RecordPercentCompletedHistogram(
      session_snapshot.progress, session_snapshot.session_duration.InMinutes());
  RecordSessionDurationHistogram(session_snapshot.time_elapsed.InMinutes());

  RecordSoundsPlayedDuringSessionHistogram(has_selected_soundscapes_,
                                           has_selected_youtube_music_);
  RecordPausedEventCountHistogram();
}

void FocusModeMetricsRecorder::RecordDNDHistogram() {
  if (has_recorded_dnd_state_histogram_) {
    return;
  }

  RecordDNDStateOnFocusEndHistogram(
      has_user_interactions_on_dnd_in_focus_session_);
  has_recorded_dnd_state_histogram_ = true;
}

void FocusModeMetricsRecorder::RecordEndingMomentBubbleHistogram(
    focus_mode_histogram_names::EndingMomentBubbleClosedReason reason) {
  base::UmaHistogramEnumeration(
      /*name=*/focus_mode_histogram_names::kEndingMomentBubbleActionHistogram,
      /*sample=*/reason);

  // If the user extends the session, then we want to make sure that the DND
  // histograms at the end of the session are triggered again.
  if (reason ==
      focus_mode_histogram_names::EndingMomentBubbleClosedReason::kExtended) {
    has_recorded_dnd_state_histogram_ = false;
  }
}

}  // namespace ash
