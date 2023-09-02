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

// The duration for window restore animation when feature Jellyroll is Enabled.
constexpr base::TimeDelta kWindowRestoreDurationCrOSNext =
    base::Milliseconds(350);

// Number of overview items needed to trigger the overview scroll layout.
constexpr int kMinimumItemsForNewLayoutInTablet = 6;
// TODO(b/286568408): Get clamshell layout number from UX.
constexpr int kMinimumItemsForNewLayoutInClamshell = 10;

// In the conceptual overview table, the horizontal space between two adjacent
// items.
constexpr int kHorizontalSpaceBetweenItemsDp = 10;

// The vertical space between two adjacent items.
constexpr int kVerticalSpaceBetweenItemsDp = 15;

// The amount we want to enlarge the dragged overview window.
constexpr int kDraggingEnlargeDp = 10;

// Height of an item header.
constexpr int kHeaderHeightDp = WindowMiniView::kHeaderHeightDp;

// Corner radius of the overview item.
constexpr int kOverviewItemCornerRadius =
    WindowMiniView::kWindowMiniViewCornerRadius;

// Windows whose aspect ratio surpass this (width twice as large as height
// or vice versa) will be classified as too wide or too tall and will be
// handled slightly differently in overview mode.
constexpr float kExtremeWindowRatioThreshold = 2.f;

// Inset for the focus ring around the focusable overview items. The ring is 2px
// thick and should have a 2px gap from the view it is associated with. Since
// the thickness is 2px and the stroke is in the middle, we use a -3px inset to
// achieve this.
constexpr int kFocusRingHaloInset = -3;

// The shadow types corresponding to the default and dragged states.
constexpr SystemShadow::Type kDefaultShadowType =
    SystemShadow::Type::kElevation12;
constexpr SystemShadow::Type kDraggedShadowType =
    SystemShadow::Type::kElevation24;

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_CONSTANTS_H_
