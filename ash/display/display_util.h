// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_UTIL_H_
#define ASH_DISPLAY_DISPLAY_UTIL_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ui/display/display.h"

namespace aura {
class Window;
}

namespace display {
class DisplayManager;
}

namespace gfx {
class Point;
class Rect;
}

namespace ash {
class AshWindowTreeHost;
class MouseWarpController;

// Creates a MouseWarpController for the current display
// configuration. |drag_source| is the window where dragging
// started, or nullptr otherwise.
std::unique_ptr<MouseWarpController> CreateMouseWarpController(
    display::DisplayManager* manager,
    aura::Window* drag_source);

// Creates edge bounds from |bounds_in_screen| that fits the edge
// of the native window for |ash_host|.
ASH_EXPORT gfx::Rect GetNativeEdgeBounds(AshWindowTreeHost* ash_host,
                                         const gfx::Rect& bounds_in_screen);

// Moves the cursor to the point inside the |ash_host| that is closest to
// the point_in_screen, which may be outside of the root window.
// |update_last_loation_now| is used for the test to update the mouse
// location synchronously.
void MoveCursorTo(AshWindowTreeHost* ash_host,
                  const gfx::Point& point_in_screen,
                  bool update_last_location_now);

// Shows the notification message for display related issues, and optionally
// adds a button to send a feedback report.
void ShowDisplayErrorNotification(const std::u16string& message,
                                  bool allow_feedback);

// Returns whether `rect_in_screen` is contained by any display.
bool IsRectContainedByAnyDisplay(const gfx::Rect& rect_in_screen);

// Takes a refresh rate represented as a float and rounds it to two decimal
// places. If the rounded refresh rate is a whole number, the mantissa is
// removed. Ex: 54.60712 -> "54.61"
ASH_EXPORT std::u16string ConvertRefreshRateToString16(float refresh_rate);

ASH_EXPORT std::u16string GetDisplayErrorNotificationMessageForTest();

// Returns whether the rotation of the source display (internal display) should
// be undone in the destination display (external display). Returning true makes
// the destination display to show in an orientation independent of the source
// display. Currently, this returns true when mirror mode is enabled in tablet
// mode (https://crbug.com/824417), or the device is in physical tablet mode
// (https://crbug.com/1180809).
bool ShouldUndoRotationForMirror();

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_UTIL_H_
