// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CONSTANTS_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CONSTANTS_H_

#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/highlight_border.h"

namespace ash::capture_mode {

constexpr gfx::Size kButtonSize{32, 32};

constexpr gfx::Insets kButtonPadding{0};

// The spacing used by the BoxLayout manager to space out child views in the
// CaptureModeBarView.
constexpr int kBetweenChildSpacing = 16;

// The amount the capture region changes when using the arrow keys to adjust it.
constexpr int kCtrlArrowKeyboardRegionChangeDp = 1;
constexpr int kArrowKeyboardRegionChangeDp = 15;
constexpr int kShiftArrowKeyboardRegionChangeDp = 40;

constexpr int kSpaceBetweenCaptureBarAndSettingsMenu = 8;

// Constants needed to paint the highlight around the area being captured.
constexpr int kCaptureRegionBorderStrokePx = 1;
// This color is set to WHITE on purpose in both dark and light mode.
constexpr SkColor kRegionBorderColor = SK_ColorWHITE;

// Color of the dimming shield layer. It is set to dim the region that is
// outside of the region will be recorded, either in the capture session or
// video recording is in progress.
constexpr ui::ColorId kDimmingShieldColor = cros_tokens::kCrosSysScrim;

// The space between the `image_toggle_button_` and `video_toggle_button_`.
constexpr int kSpaceBetweenCaptureModeTypeButtons = 2;

// The minimum value we use to clamp the camera preview diameter.
constexpr int kMinCameraPreviewDiameter = 120;

// The minimum value if the diameter of the expanded camera preview goes below
// which, the camera preview will be considered too small to be collapsible.
constexpr int kMinCollapsibleCameraPreviewDiameter = 150;

// The minimum value of the shorter side of the surface within which the camera
// preview is confined. Less values will cause the camera preview to hide.
constexpr int kMinCaptureSurfaceShortSideLengthForVisibleCamera = 176;

// The value by which we divide the shorter side of the surface within which the
// camera preview is confined (e.g. the display work area when recrding the
// fullscreen) to calculate the expanded diameter.
constexpr int kCaptureSurfaceShortSideDivider = 4;

// The divider used to calculate the collapsed diameter from the expanded
// diameter.
constexpr int kCollapsedPreviewDivider = 2;

// Size of the camera preview border.
constexpr int kCameraPreviewBorderSize = 4;

// The space between the camera preview and edges of the bounds that will be
// recorded.
constexpr int kSpaceBetweenCameraPreviewAndEdges = 16;

// The space between the bottom of camera preview resize button and the bottom
// of the camera preview.
constexpr int kSpaceBetweenResizeButtonAndCameraPreview = 12;

// The duration to continue showing resize button since the mouse exiting the
// preview bounds or the last tap on the preview widget.
constexpr base::TimeDelta kResizeButtonShowDuration = base::Milliseconds(4500);

// When capture UI (capture bar, capture label) is overlapped with user
// capture region or camera preview, and the mouse is not hovering over the
// capture UI, drop the opacity to this value to make the region or camera
// preview easier to see.
constexpr float kCaptureUiOverlapOpacity = 0.15f;

// Size of the icon in the capture mode settings menu.
constexpr gfx::Size kSettingsIconSize{20, 20};

// Border value used for each section of the settings menu.
constexpr auto kSettingsMenuBorderSize = gfx::Insets::VH(8, 16);

// The distance between the bottom of the key combo viewer and the bottom of the
// confined bounds.
constexpr int kKeyWidgetDistanceFromBottom = 24;

// Border value applied between the right edge of the key combo viewer when it's
// approaching the right edge of the recording area.
constexpr int kKeyWidgetBorderPadding = 16;

// The duration to continue showing the key combo view on key up of the
// non-modifier key with no modifier keys pressed or on key up of the last
// modifier key up with no non-modifier key that can be displayed independently
// pressed.
constexpr base::TimeDelta kRefreshKeyComboWidgetLongDelay =
    base::Milliseconds(1500);

// The duration to hold on the update of the key combo view on key up.
constexpr base::TimeDelta kRefreshKeyComboWidgetShortDelay =
    base::Milliseconds(100);

// The radius of the highlight layer generated on mouse or touch event when the
// demo tools feature is enabled.
constexpr int kHighlightLayerRadius = 36;

// The thickness of the highlight border that will be applied to the
// `PointerHighlightLayer`.
constexpr float kInnerHightlightBorderThickness =
    0.5 * views::kHighlightBorderThickness;
constexpr float kOuterHightlightBorderThickness =
    1.5 * views::kHighlightBorderThickness;

// Distance from the bottom of the capture bar to the bottom of the anchor
// bounds of the bar. See `GetBarAnchorBoundsInScreen` for more details of the
// bar's anchor bounds.
constexpr int kCaptureBarBottomPadding = 16;
constexpr int kGameCaptureBarBottomPadding = 24;

// Height of the capture bar.
constexpr int kCaptureBarHeight = 64;

// The minimum distance from the top of the screen to the top of the settings
// menu.
constexpr int kMinDistanceFromSettingsToScreen = 8;

// The minimum height of the settings menu while constrained and scrollable.
constexpr int kSettingsMenuMinHeight = 50;

}  // namespace ash::capture_mode

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CONSTANTS_H_
