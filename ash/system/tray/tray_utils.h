// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_UTILS_H_
#define ASH_SYSTEM_TRAY_TRAY_UTILS_H_

#include <cstdint>
#include <optional>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_popup_ink_drop_style.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"

namespace aura {
class Window;
}

namespace views {
class Label;
}

namespace ash {

class HoverHighlightView;
class TrayBackgroundView;

// Sets up a Label properly for the tray (sets color, font etc.).
void SetupLabelForTray(views::Label* label);

// Adds connected sub label to the |view| with appropriate style and updates
// accessibility label.
void SetupConnectedScrollListItem(HoverHighlightView* view);

// Adds connected sub label with the device's battery percentage to the |view|
// with appropriate style and updates accessibility label.
void SetupConnectedScrollListItem(HoverHighlightView* view,
                                  std::optional<uint8_t> battery_percentage);

// Adds connecting sub label to the |view| with appropriate style and updates
// accessibility label.
void SetupConnectingScrollListItem(HoverHighlightView* view);

// Add `subtext` with warning color to `view`.
void SetWarningSubText(HoverHighlightView* view, std::u16string subtext);

// Returns the insets above the shelf for the display containing `window` for
// positioning the quick settings bubble.
gfx::Insets GetTrayBubbleInsets(aura::Window* window);

// Calculates the height compensations in tablet mode based on whether the
// hotseat for the display containing `window` is shown.
int GetBubbleInsetHotseatCompensation(aura::Window* window);

// Gets the InkDrop insets based on `ink_drop_style`.
gfx::Insets GetInkDropInsets(TrayPopupInkDropStyle ink_drop_style);

// Gets the maximum height possible for a tray bubble that would be shown in the
// display containing `window` based on that display's available screen space.
int CalculateMaxTrayBubbleHeight(aura::Window* window);

// Creates a default instance of InitParams for a tray bubble. If
// `anchor_to_shelf_corner` is true, the bubble will be anchored to the corner
// of the shelf, near the status area button. Otherwise, it will be anchored to
// the associated `tray`.
TrayBubbleView::InitParams ASH_EXPORT
CreateInitParamsForTrayBubble(TrayBackgroundView* tray,
                              bool anchor_to_shelf_corner = false);

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_UTILS_H_
