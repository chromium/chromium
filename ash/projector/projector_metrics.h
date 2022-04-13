// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_METRICS_H_
#define ASH_PROJECTOR_PROJECTOR_METRICS_H_

#include <cstddef>

namespace ash {

// These enum values represent buttons on the Projector toolbar and log to UMA.
// Entries should not be renumbered and numeric values should never be reused.
// Please keep in sync with "ProjectorToolbar" in
// src/tools/metrics/histograms/enums.xml.
enum class ProjectorToolbar {
  kToolbarOpened = 0,
  kToolbarClosed = 1,
  kKeyIdea = 2,
  kLaserPointer = 3,
  kMarkerTool = 4,
  kExpandMarkerTools = 5,
  kCollapseMarkerTools = 6,
  kClearAllMarkers = 7,
  kStartMagnifier = 8,
  kStopMagnifier = 9,
  kStartSelfieCamera = 10,
  kStopSelfieCamera = 11,
  kStartClosedCaptions = 12,
  kStopClosedCaptions = 13,
  kToolbarLocationBottomLeft = 14,
  kToolbarLocationTopLeft = 15,
  kToolbarLocationTopRight = 16,
  kToolbarLocationBottomRight = 17,
  kUndo = 18,
  kToolbarLocationTopCenter = 19,
  kToolbarLocationBottomCenter = 20,
  // Add future entries above this comment, in sync with
  // "ProjectorToolbar" in src/tools/metrics/histograms/enums.xml.
  // Update kMaxValue to the last value.
  kMaxValue = kToolbarLocationBottomCenter
};

// These enum values represent marker colors on the Projector toolbar and log to
// UMA. Entries should not be renumbered and numeric values should never be
// reused. Please keep in sync with "ProjectorMarkerColor" in
// src/tools/metrics/histograms/enums.xml.
enum class ProjectorMarkerColor {
  kBlack = 0,
  kWhite = 1,
  kBlue = 2,
  kRed = 3,
  kYellow = 4,
  kMagenta = 5,
  // Add future entries above this comment, in sync with
  // "ProjectorMarkerColor" in src/tools/metrics/histograms/enums.xml.
  // Update kMaxValue to the last value.
  kMaxValue = kMagenta
};

// These enum values represent steps in the Projector creation flow and log to
// UMA. Entries should not be renumbered and numeric values should never be
// reused. Please keep in sync with "ProjectorCreationFlow" in
// src/tools/metrics/histograms/enums.xml.
enum class ProjectorCreationFlow {
  kSessionStarted = 0,
  kRecordingStarted = 1,
  kRecordingAborted = 2,
  kRecordingEnded = 3,
  kSessionStopped = 4,
  // Add future entries above this comment, in sync with
  // "ProjectorCreationFlow" in src/tools/metrics/histograms/enums.xml.
  // Update kMaxValue to the last value.
  kMaxValue = kSessionStopped
};

// These enum values represent user-facing errors in the Projector creation flow
// and log to UMA. Entries should not be renumbered and numeric values should
// never be reused. Please keep in sync with "ProjectorCreationFlowError" in
// src/tools/metrics/histograms/enums.xml.
enum class ProjectorCreationFlowError {
  kSaveError = 0,
  kTranscriptionError = 1,
  // Add future entries above this comment, in sync with
  // "ProjectorCreationFlowError" in src/tools/metrics/histograms/enums.xml.
  // Update kMaxValue to the last value.
  kMaxValue = kTranscriptionError
};

// Records the buttons the user presses on the Projector toolbar.
void RecordToolbarMetrics(ProjectorToolbar button);

// Records the marker colors the user chooses to use. Only records if the user
// switches from the default color.
void RecordMarkerColorMetrics(ProjectorMarkerColor color);

// Records the user's progress in the Projector creation flow.
void RecordCreationFlowMetrics(ProjectorCreationFlow step);

// Records the number of transcripts generated during a screencast recording.
void RecordTranscriptsCount(size_t count);

// Records errors encountered during the creation flow.
void RecordCreationFlowError(int message_id);

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_METRICS_H_
