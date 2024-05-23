// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_metrics_recorder.h"

#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_histogram_names.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
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
      NOTREACHED_IN_MIGRATION();
  }
}

void RecordHasSelectedTaskOnSessionStartHistogram() {
  base::UmaHistogramBoolean(
      /*name=*/focus_mode_histogram_names::
          kHasSelectedTaskOnSessionStartHistogramName,
      /*sample=*/FocusModeController::Get()->HasSelectedTask());
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

void FocusModeMetricsRecorder::RecordHistogramsOnStart(
    focus_mode_histogram_names::ToggleSource source) {
  RecordInitialDurationHistogram(
      /*session_duration=*/initial_session_duration_);
  RecordStartSessionSourceHistogram(source);
  RecordHasSelectedTaskOnSessionStartHistogram();
}

void FocusModeMetricsRecorder::RecordHistogramsOnEnd() {
  RecordTasksSelectedHistogram(tasks_selected_count_);
  RecordTasksCompletedHistogram(tasks_completed_count_);
  RecordDNDStateOnFocusEndHistogram(
      has_user_interactions_on_dnd_in_focus_session_);

  auto session_snapshot =
      FocusModeController::Get()->GetSnapshot(base::Time::Now());
  RecordTimeAddedOnSessionEndHistogram(
      initial_session_duration_.InMinutes(),
      session_snapshot.session_duration.InMinutes());
  RecordPercentCompletedHistogram(
      session_snapshot.progress, session_snapshot.session_duration.InMinutes());
  RecordSessionDurationHistogram(session_snapshot.time_elapsed.InMinutes());
}

void FocusModeMetricsRecorder::RecordHistogramOnEndingMoment(
    focus_mode_histogram_names::EndingMomentBubbleClosedReason reason) {
  base::UmaHistogramEnumeration(
      /*name=*/focus_mode_histogram_names::kEndingMomentBubbleActionHistogram,
      /*sample=*/reason);
}

}  // namespace ash
