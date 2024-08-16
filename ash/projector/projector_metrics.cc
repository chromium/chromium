// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_metrics.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"

namespace ash {

namespace {

constexpr char kProjectorToolbarHistogramName[] = "Ash.Projector.Toolbar";

constexpr char kProjectorCreationFlowHistogramName[] =
    "Ash.Projector.CreationFlow";

constexpr char kProjectorCreationFlowErrorHistogramName[] =
    "Ash.Projector.CreationFlowError";

constexpr char kProjectorTranscriptsCountHistogramName[] =
    "Ash.Projector.TranscriptsCount";

constexpr char kProjectorPendingScreencastBatchIOTaskDurationHistogramName[] =
    "Ash.Projector.PendingScreencastBatchIOTaskDuration";

constexpr char kProjectorPendingScreencastChangeIntervalHistogramName[] =
    "Ash.Projector.PendingScreencastChangeInterval";

constexpr char
    kProjectorOnDeviceToServerSpeechRecognitionFallbackReasonHistogramName[] =
        "Ash.Projector.OnDeviceToServerSpeechRecognitionFallbackReason";

constexpr char kSpeechRecognitionEndStateOnDevice[] =
    "Ash.Projector.SpeechRecognitionEndState.OnDevice";

constexpr char kSpeechRecognitionEndStateServerBased[] =
    "Ash.Projector.SpeechRecognitionEndState.ServerBased";

// Appends the proper suffix to |prefix| based on whether the user is in tablet
// mode or not.
std::string GetHistogramName(const std::string& prefix) {
  std::string mode =
      Shell::Get()->IsInTabletMode() ? ".TabletMode" : ".ClamshellMode";
  return prefix + mode;
}

inline std::string GetSpeechRecognitionHistogramName(bool is_on_device) {
  return is_on_device ? kSpeechRecognitionEndStateOnDevice
                      : kSpeechRecognitionEndStateServerBased;
}

}  // namespace

void RecordToolbarMetrics(ProjectorToolbar button) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kProjectorToolbarHistogramName), button);
}

void RecordCreationFlowMetrics(ProjectorCreationFlow step) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kProjectorCreationFlowHistogramName), step);
}

void RecordTranscriptsCount(size_t count) {
  // We don't expect most screencasts to exceed 10,000 transcripts. If this
  // limit is exceeded, then the metric would fall into an overflow bucket.
  base::UmaHistogramCounts10000(
      GetHistogramName(kProjectorTranscriptsCountHistogramName), count);
}

void RecordCreationFlowError(int message_id) {
  ProjectorCreationFlowError error = ProjectorCreationFlowError::kMaxValue;
  switch (message_id) {
    case IDS_ASH_PROJECTOR_SAVE_FAILURE_TEXT:
      error = ProjectorCreationFlowError::kSaveError;
      break;
    case IDS_ASH_PROJECTOR_FAILURE_MESSAGE_TRANSCRIPTION:
      error = ProjectorCreationFlowError::kTranscriptionError;
      break;
    case IDS_ASH_PROJECTOR_ABORT_BY_AUDIO_POLICY_TEXT:
      error = ProjectorCreationFlowError::kSessionAbortedByAudioPolicyDisabled;
      break;
    default:
      NOTREACHED();
  }
  base::UmaHistogramEnumeration(
      GetHistogramName(kProjectorCreationFlowErrorHistogramName), error);
}

ASH_EXPORT void RecordPendingScreencastBatchIOTaskDuration(
    const base::TimeDelta duration) {
  // We don't normally expect the duration is longer than 10s. If this limit is
  // exceeded, then the metric would fall into an overflow bucket.
  base::UmaHistogramTimes(
      kProjectorPendingScreencastBatchIOTaskDurationHistogramName, duration);
}

ASH_EXPORT void RecordPendingScreencastChangeInterval(
    const base::TimeDelta interval) {
  // The interval doesn't include the change between last finished uploading to
  // new start uploading. We don't normally expect the interval is longer than
  // 10s. If this limit is exceeded, then the metric would fall into an overflow
  // bucket.
  base::UmaHistogramTimes(
      kProjectorPendingScreencastChangeIntervalHistogramName, interval);
}

ASH_EXPORT void RecordOnDeviceToServerSpeechRecognitionFallbackReason(
    OnDeviceToServerSpeechRecognitionFallbackReason reason) {
  base::UmaHistogramEnumeration(
      kProjectorOnDeviceToServerSpeechRecognitionFallbackReasonHistogramName,
      reason);
}

ASH_EXPORT void RecordSpeechRecognitionEndState(
    SpeechRecognitionEndState end_state,
    bool is_on_device) {
  base::UmaHistogramEnumeration(GetSpeechRecognitionHistogramName(is_on_device),
                                end_state);
}

}  // namespace ash
