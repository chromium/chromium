// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

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

// Notification Utils //
// Constants related to the banner view on the image capture notifications.
constexpr int kBannerHeightDip = 36;
constexpr int kBannerHorizontalInsetDip = 12;
constexpr int kBannerVerticalInsetDip = 8;
constexpr int kBannerIconTextSpacingDip = 8;
constexpr int kBannerIconSizeDip = 20;

// Constants related to the play icon view for video capture notifications.
constexpr int kPlayIconSizeDip = 24;
constexpr int kPlayIconBackgroundCornerRadiusDip = 20;
constexpr gfx::Size kPlayIconViewSize{40, 40};

std::unique_ptr<views::View> CreateClipboardShortcutView();
// Creates the banner view that will show on top of the notification image.
std::unique_ptr<views::View> CreateBannerView();

// Creates the play icon view which shows on top of the video thumbnail in the
// notification.
std::unique_ptr<views::View> CreatePlayIconView();

}  // namespace capture_mode_util

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_
