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

constexpr float kInkDropVisibleOpacity = 0.2f;

constexpr float kInkDropHighlightVisibleOpacity = 0.3f;

constexpr SkColor kInkDropBaseColor = SK_ColorWHITE;

// The spacing used by the BoxLayout manager to space out child views in the
// CaptureModeBarView.
constexpr int kBetweenChildSpacing = 16;

// The amount the capture region changes when using the arrow keys to adjust it.
constexpr int kArrowKeyboardRegionChangeDp = 1;
constexpr int kShiftArrowKeyboardRegionChangeDp = 10;

}  // namespace capture_mode

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CONSTANTS_H_
