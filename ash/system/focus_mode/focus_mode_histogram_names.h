// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_HISTOGRAM_NAMES_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_HISTOGRAM_NAMES_H_

namespace ash::focus_mode_histogram_names {

// Histograms recorded when starting a session.
constexpr char kHasSelectedTaskOnSessionStartHistogramName[] =
    "Ash.FocusMode.StartSession.HasSelectedTask";
constexpr char kInitialDurationOnSessionStartsHistogramName[] =
    "Ash.FocusMode.StartSession.InitialDuration";
constexpr char kStartSessionSourceHistogramName[] =
    "Ash.FocusMode.StartSession.ToggleSource";

// Histograms recorded during a session.
constexpr char kToggleEndButtonDuringSessionHistogramName[] =
    "Ash.FocusMode.DuringSession.ToggleEndSessionSource";

// Histograms recorded when a session ends.
constexpr char kTasksSelectedHistogramName[] = "Ash.FocusMode.TasksSelected";
constexpr char kDNDStateOnFocusEndHistogramName[] =
    "Ash.FocusMode.DNDStateOnFocusEnd";

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

}  // namespace ash::focus_mode_histogram_names

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_HISTOGRAM_NAMES_H_
