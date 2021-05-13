// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ash {

namespace capture_mode_util {

// Returns true if the capture mode feature is enabled and capture mode is
// active. This method allows callers to avoid including the full header for
// CaptureModeController, which has many transitive includes.
bool IsCaptureModeActive();

// Retrieves the point on the |rect| associated with |position|.
ASH_EXPORT gfx::Point GetLocationForFineTunePosition(const gfx::Rect& rect,
                                                     FineTunePosition position);

// Return whether |position| is a corner.
bool IsCornerFineTunePosition(FineTunePosition position);

// Sets the visibility of the stop-recording button in the Shelf's status area
// widget of the given |root| window.
void SetStopRecordingButtonVisibility(aura::Window* root, bool visible);

// Triggers an accessibility alert to give the user feedback.
void TriggerAccessibilityAlert(const std::string& message);
void TriggerAccessibilityAlert(int message_id);

}  // namespace capture_mode_util

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_
