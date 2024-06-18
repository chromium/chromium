// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_METRICS_H_
#define ASH_PROJECTOR_PROJECTOR_METRICS_H_

#include <cstddef>

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {

// These enum values represent buttons on the Projector toolbar and log to UMA.
// Entries should not be renumbered and numeric values should never be reused.
// When removing an unused enumerator, comment it out, making it clear the value
// was previously used.
// Please keep in sync with "ProjectorToolbar" in
// src/tools/metrics/histograms/metadata/ash/enums.xml.
enum class ProjectorToolbar {
  // kToolbarOpened = 0,
  // kToolbarClosed = 1,
  // kKeyIdea = 2,
  // kLaserPointer = 3,
  kMarkerTool = 4,
  // kExpandMarkerTools = 5,
  // kCollapseMarkerTools = 6,
  // kClearAllMarkers = 7,
  // kStartMagnifier = 8,
  // kStopMagnifier = 9,
  // kStartSelfieCamera = 10,
  // kStopSelfieCamera = 11,
  // kStartClosedCaptions = 12,
  // kStopClosedCaptions = 13,
  // kToolbarLocationBottomLeft = 14,
  // kToolbarLocationTopLeft = 15,
  // kToolbarLocationTopRight = 16,
  // kToolbarLocationBottomRight = 17,
  // kUndo = 18,
  // kToolbarLocationTopCenter = 19,
  // kToolbarLocationBottomCenter = 20,
  // Add future entries above this comment, in sync with
  // "ProjectorToolbar" in src/tools/metrics/histograms/metadata/ash/enums.xml.
  // Update kMaxValue to the last value.
  kMaxValue = kMarkerTool
};

// These enum values represent steps in the Projector creation flow and log to
// UMA. Entries should not be renumbered and numeric values should never be
// reused. Please keep in sync with "ProjectorCreationFlow" in
// src/tools/metrics/histograms/metadata/ash/enums.xml.
enum class ProjectorCreationFlow {
  kSessionStarted = 0,
  kRecordingStarted = 1,
  kRecordingAborted = 2,
  kRecordingEnded = 3,
  kSessionStopped = 4,
  // Add future entries above this comment, in sync with
  // "ProjectorCreationFlow" in
  // src/tools/metrics/histograms/metadata/ash/enums.xml. Update kMaxValue to
  // the last value.
  kMaxValue = kSessionStopped
};

// These enum values represent user-facing errors in the Projector creation flow
// and log to UMA. Entries should not be renumbered and numeric values should
// never be reused. Please keep in sync with "ProjectorCreationFlowError" in
// src/tools/metrics/histograms/metadata/ash/enums.xml.
enum class ProjectorCreationFlowError {
  kSaveError = 0,
  kTranscriptionError = 1,
  kSessionAbortedByAudioPolicyDisabled = 2,
  // Add future entries above this comment, in sync with
  // "ProjectorCreationFlowError" in
  // src/tools/metrics/histograms/metadata/ash/enums.xml. Update kMaxValue to
  // the last value.
  kMaxValue = kSessionAbortedByAudioPolicyDisabled
};

// These enum values represent potential error that occurs at policy value
// change handling and log to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "OnDeviceToServerSpeechRecognitionFallbackReason" in
// src/tools/metrics/histograms/metadata/ash/enums.xml.
// This enum is the smiliar to the `OnDeviceRecognitionAvailability` because
// all fallback reasons are related to on device recognition is not supported.
enum class OnDeviceToServerSpeechRecognitionFallbackReason : int {
  // Device does not support SODA (Speech on Device API)
  kSodaNotAvailable = 0,
  // User's language is not supported by SODA.
  kUserLanguageNotAvailableForSoda = 1,
  // SODA binary is not yet installed.
  kSodaNotInstalled = 2,
  // SODA binary and language packs are downloading.
  kSodaInstalling = 3,
  // SODA installation failed.
  kSodaInstallationErrorUnspecified = 4,
  // SODA installation error needs reboot
  kSodaInstallationErrorNeedsReboot = 5,
  // Server based speech recognition is enforced by flag for dev purpose.
  kEnforcedByFlag = 6,

  kMaxValue = kEnforcedByFlag,
};

// Enum class to record metric for speech recognition status.
// This enum should never be reused as it is being logged into UMA.
enum class SpeechRecognitionEndState {
  // Speech recognition successfully stopped.
  kSpeechRecognitionSuccessfullyStopped = 0,
  // Speech recognition encountered error while recording was taking place.
  kSpeechRecognitionEnounteredError = 1,
  // Speech recognition encountered error while attempting to stop.
  kSpeechRecognitionEncounteredErrorWhileStopping = 2,
  // Speech recognition has been forced stopped.
  kSpeechRecognitionForcedStopped = 3,

  kMaxValue = kSpeechRecognitionForcedStopped,
};

// Records the buttons the user presses on the Projector toolbar.
void RecordToolbarMetrics(ProjectorToolbar button);

// Records the user's progress in the Projector creation flow.
void RecordCreationFlowMetrics(ProjectorCreationFlow step);

// Records the number of transcripts generated during a screencast recording.
void RecordTranscriptsCount(size_t count);

// Records errors encountered during the creation flow.
void RecordCreationFlowError(int message_id);

// Records the IO task processing Time for screencast validation.
void RecordPendingScreencastBatchIOTaskDuration(const base::TimeDelta duration);

// Records the interval between the UI changes of pending screencasts.
void RecordPendingScreencastChangeInterval(const base::TimeDelta interval);

void RecordOnDeviceToServerSpeechRecognitionFallbackReason(
    OnDeviceToServerSpeechRecognitionFallbackReason reason);

void RecordSpeechRecognitionEndState(SpeechRecognitionEndState state,
                                     bool is_on_device);

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_METRICS_H_
