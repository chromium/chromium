// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CONSTANTS_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CONSTANTS_H_

#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

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
constexpr SkColor kRegionBorderColor = SK_ColorWHITE;

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

// The space between the camera preview and edges of the bounds that will be
// recorded.
constexpr int kSpaceBetweenCameraPreviewAndEdges = 16;

// The space between the bottom of camera preview resize button and the bottom
// of the camera preview.
constexpr int kSpaceBetweenResizeButtonAndCameraPreview = 12;

// The duration to continue showing resize button since the mouse exiting the
// preview bounds or the last tap on the preview widget.
constexpr base::TimeDelta kResizeButtonShowDuration = base::Milliseconds(4500);

}  // namespace ash::capture_mode

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CONSTANTS_H_
