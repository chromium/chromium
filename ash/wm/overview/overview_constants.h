// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_CONSTANTS_H_
#define ASH_WM_OVERVIEW_OVERVIEW_CONSTANTS_H_

#include "ash/style/system_shadow.h"
#include "ash/wm/window_mini_view.h"
#include "base/time/time.h"

namespace ash {

// The time duration for transformation animations.
constexpr base::TimeDelta kTransition = base::Milliseconds(300);

// In the conceptual overview table, the horizontal space between two adjacent
// items.
constexpr int kHorizontalSpaceBetweenItemsDp = 10;

// The vertical space between two adjacent items.
constexpr int kVerticalSpaceBetweenItemsDp = 15;

// The amount we want to enlarge the dragged overview window.
constexpr int kDraggingEnlargeDp = 10;

// Windows whose aspect ratio surpass this (width twice as large as height
// or vice versa) will be classified as too wide or too tall and will be
// handled slightly differently in overview mode.
constexpr float kExtremeWindowRatioThreshold = 2.f;

// The shadow types corresponding to the default and dragged states.
constexpr SystemShadow::Type kDefaultShadowType =
    SystemShadow::Type::kElevation12;
constexpr SystemShadow::Type kDraggedShadowType =
    SystemShadow::Type::kElevation24;

// Rounded corner radii applied on the wallpaper clip rect.
constexpr gfx::RoundedCornersF kWallpaperClipRoundedCornerRadii(20.f);

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_CONSTANTS_H_
