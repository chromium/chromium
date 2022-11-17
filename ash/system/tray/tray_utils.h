// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_UTILS_H_
#define ASH_SYSTEM_TRAY_TRAY_UTILS_H_

#include <cstdint>

#include "ash/system/tray/tray_popup_ink_drop_style.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"

namespace views {
class Label;
}

namespace ash {

class HoverHighlightView;

// Sets up a Label properly for the tray (sets color, font etc.).
void SetupLabelForTray(views::Label* label);

// Adds connected sub label to the |view| with appropriate style and updates
// accessibility label.
void SetupConnectedScrollListItem(HoverHighlightView* view);

// Adds connected sub label with the device's battery percentage to the |view|
// with appropriate style and updates accessibility label.
void SetupConnectedScrollListItem(HoverHighlightView* view,
                                  absl::optional<uint8_t> battery_percentage);

// Adds connecting sub label to the |view| with appropriate style and updates
// accessibility label.
void SetupConnectingScrollListItem(HoverHighlightView* view);

// Add `subtext` with warning color to `view`.
void SetWarningSubText(HoverHighlightView* view, std::u16string subtext);

// Returns the insets above the shelf for positioning the quick settings bubble.
gfx::Insets GetTrayBubbleInsets();

// Calculates the height compensations in tablet mode based on whether the
// hotseat is shown.
int GetBubbleInsetHotseatCompensation();

// Returns the separation above the shelf for positioning secondary tray
// bubbles. (Palette Tray, IME Tray).
gfx::Insets GetSecondaryBubbleInsets();

// Gets the InkDrop insets based on `ink_drop_style`.
gfx::Insets GetInkDropInsets(TrayPopupInkDropStyle ink_drop_style);

// Gets the maximum height possible for a tray bubble based on the available
// screen space.
int CalculateMaxTrayBubbleHeight();

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_UTILS_H_
