// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_HISTOGRAM_NAMES_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_HISTOGRAM_NAMES_H_

namespace ash::focus_mode_histogram_names {

constexpr char kShortSuffix[] = "Short";
constexpr char kMediumSuffix[] = "Medium";
constexpr char kLongSuffix[] = "Long";

// Histograms recorded when starting a session.
constexpr char kInitialDurationOnSessionStartsHistogramName[] =
    "Ash.FocusMode.StartSession.InitialDuration";
constexpr char kStartSessionSourceHistogramName[] =
    "Ash.FocusMode.StartSession.ToggleSource";
constexpr char kStartedWithTaskStatekHistogramName[] =
    "Ash.FocusMode.StartSession.TaskState";
constexpr char kStartedWithExistingMediaPlayingHistogramName[] =
    "Ash.FocusMode.StartSession.ExistingMediaPlaying";

// Histograms recorded during a session.
constexpr char kToggleEndButtonDuringSessionHistogramName[] =
    "Ash.FocusMode.DuringSession.ToggleEndSessionSource";
constexpr char kSoundscapeLatencyInMillisecondsHistogramName[] =
    "Ash.FocusMode.SoundscapeLatency";
constexpr char kYouTubeMusicLatencyInMillisecondsHistogramName[] =
    "Ash.FocusMode.YouTubeMusicLatency";
constexpr char kPlaylistChosenHistogram[] = "Ash.FocusMode.PlaylistChosen";

// Histograms recorded when a session ends.
constexpr char kTasksSelectedHistogramName[] = "Ash.FocusMode.TasksSelected";
constexpr char kDNDStateOnFocusEndHistogramName[] =
    "Ash.FocusMode.DNDStateOnFocusEnd";
constexpr char kTimeAddedOnSessionEndPrefix[] = "Ash.FocusMode.TimeAdded.";
constexpr char kPercentCompletedPrefix[] =
    "Ash.FocusMode.PercentOfSessionCompleted.";
constexpr char kTasksCompletedHistogramName[] = "Ash.FocusMode.TasksCompleted";
constexpr char kSessionDurationHistogramName[] =
    "Ash.FocusMode.SessionDuration";
constexpr char kEndingMomentBubbleActionHistogram[] =
    "Ash.FocusMode.EndingMomentBubbleAction";
constexpr char kPlaylistTypesSelectedDuringSession[] =
    "Ash.FocusMode.PlaylistTypesSelectedDuringSession";
constexpr char kCountPlaylistsPlayedDuringSession[] =
    "Ash.FocusMode.PlaylistsDuringSession";
constexpr char kMusicPausedEventsCount[] =
    "Ash.FocusMode.MusicPausedSessionCount";

// Format strings for API related histograms.
inline constexpr char kApiStatus[] = "Ash.FocusMode.Api.%s.Status";
inline constexpr char kApiLatency[] = "Ash.FocusMode.Api.%s.Latency";
inline constexpr char kApiResult[] = "Ash.FocusMode.Api.%s.Result";
inline constexpr char kApiRetryCount[] = "Ash.FocusMode.Api.%s.Latency";

// This enum is used for metrics, so enum values should not be changed. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
// This should be kept in sync with `FocusModeEndSessionSource` enum in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class ToggleSource {
  kFocusPanel = 0,       // Toggle focus mode through the focus panel.
  kContextualPanel = 1,  // Toggle focus mode through the contextual panel.
  kFeaturePod = 2,       // Toggle focus mode through the feature pod in quick
                         // settings.
  kMaxValue = kFeaturePod,
};

// This enum is used for metrics, so enum values should not be changed. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
// This should be kept in sync with `FocusModeStartSessionSource` enum in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class StartSessionSource {
  kFocusPanel = 0,  // Toggle focus mode through the focus panel.
  kFeaturePod = 1,  // Toggle focus mode through the feature pod in quick
                    // settings.
  kMaxValue = kFeaturePod,
};

// This enum is used for metrics, so enum values should not be changed. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
// This should be kept in sync with `DNDStateOnFocusEndType` enum in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class DNDStateOnFocusEndType {
  kFocusModeOn = 0,  // DND enabled by FocusMode (default behavior).
  kAlreadyOn =
      1,  // DND was already on before FocusMode started and was on when we
          // finished (with NO user interaction during the session).
  kAlreadyOff = 2,  // DND was off when FocusMode started, and is still off
                    // (with NO user interactions during the session).
  kTurnedOn = 3,  // The user manually toggled DND during the focus session, and
                  // the session ends with DND on.
  kTurnedOff = 4,  // The user manually toggled DND during the focus session,
                   // and the session ends with DND off.
  kMaxValue = kTurnedOff,
};

// This enum is used for metrics, so enum values should not be changed. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
// This should be kept in sync with `FocusModeEndingMomentBubbleClosedReason`
// enum in tools/metrics/histograms/metadata/ash/enums.xml.
enum class EndingMomentBubbleClosedReason {
  kIgnored = 0,   // Bubble was never opened.
  kExtended = 1,  // Bubble was opened and minutes were added to the session.
  kOpended = 2,   // Bubble was opened but no action was taken.
  kMaxValue = kOpended,
};

// This enum is used for metrics, so enum values should not be changed. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
// This should be kept in sync with `FocusModeStartedWithTaskState` enum in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class StartedWithTaskState {
  kNoTask = 0,  // Start a session without a selected task.
  kPreviouslySelectedTask =
      1,  // Start a session with a selected task which was selected in the
          // previous focus session and hasn't been completed by the end of the
          // previous session.
  kNewlySelectedTask = 2,  // Start a session with a selected task which isn't
                           // `kPreviouslySelectedTask` type.
  kMaxValue = kNewlySelectedTask,
};

// This enum is used for metrics, so enum values should not be changed. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
// This should be kept in sync with `PlaylistTypesSelectedDuringSessionType`
// enum in tools/metrics/histograms/metadata/ash/enums.xml.
enum class PlaylistTypesSelectedDuringFocusSessionType {
  kNone = 0,
  kYouTubeMusic = 1,
  kSoundscapes = 2,
  kYouTubeMusicAndSoundscapes = 3,
  kMaxValue = kYouTubeMusicAndSoundscapes,
};

// This enum is used for metrics, so enum values should not be changed. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
// This should be kept in sync with `FocusModePlaylistChosen` enum in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class FocusModePlaylistChosen {
  kNone = 0,
  kSoundscapes1 = 1,
  kSoundscapes2 = 2,
  kSoundscapes3 = 3,
  kSoundscapes4 = 4,
  kYouTubeMusic1 = 5,
  kYouTubeMusic2 = 6,
  kYouTubeMusic3 = 7,
  kYouTubeMusic4 = 8,
  kMaxValue = kYouTubeMusic4,
};

}  // namespace ash::focus_mode_histogram_names

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_HISTOGRAM_NAMES_H_
