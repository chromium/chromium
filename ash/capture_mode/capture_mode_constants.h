// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CONSTANTS_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

namespace capture_mode {

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

constexpr gfx::Size kCameraPreviewSize{96, 96};

// The space between the camera preview and edges of the bounds that will be
// recorded.
constexpr int kSpaceBetweenCameraPreviewAndEdges = 16;

}  // namespace capture_mode

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CONSTANTS_H_
