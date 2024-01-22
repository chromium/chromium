// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_HISTOGRAM_NAMES_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_HISTOGRAM_NAMES_H_

namespace ash::focus_mode_histogram_names {

// This enum is used for metrics, so enum values should not be changed. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum class ToggleSource {
  kFocusPanel = 0,       // Toggle focus mode through the focus panel.
  kContextualPanel = 1,  // Toggle focus mode through the contextual panel.
  kFeaturePod = 2,       // Toggle focus mode through the feature pod in quick
                         // settings.
  kMaxValue = kFeaturePod,
};

// Histogram names during session.
constexpr char kToggleEndButtonDuringSessionHistogramName[] =
    "Ash.FocusMode.DuringSession.ToggleEndSessionSource";

}  // namespace ash::focus_mode_histogram_names

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_HISTOGRAM_NAMES_H_
