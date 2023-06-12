// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_CONSTANTS_H_
#define ASH_WM_DESKS_DESKS_CONSTANTS_H_

#include "base/time/time.h"

namespace ash {

// The space between the starting and ending desks screenshots in dips.
constexpr int kDesksSpacing = 50;

// When the user tries to navigate in a direction where there is no desk, we
// allow shifting the desk a certain amount. The amount is a percentage of the
// root window's width. This padding is used by both the hit the wall animation
// and while swiping.
constexpr float kEdgePaddingRatio = 0.1f;

// In touchpad units, a touchpad swipe of this length will correspond to a
// full desk change.
constexpr int kTouchpadSwipeLengthForDeskChange = 420;

// This is the height allocated for elements other than the desk preview (e.g.
// the DeskNameView, and the vertical paddings). Note, the vertical paddings
// should exclude the preview border's insets.
constexpr int kDeskBarNonPreviewAllocatedHeight = 48;

// This is the desk bar height for zero state.
constexpr int kDeskBarZeroStateHeight = 40;

constexpr int kDeskBarGradientZoneLength = 40;

constexpr int kDeskBarDeskPreviewViewFocusRingThicknessAndPadding = 4;

// The minimum horizontal padding of the scroll view. This is set to make sure
// there is enough space for the scroll buttons.
constexpr int kDeskBarScrollViewMinimumHorizontalPaddingOverview = 32;
constexpr int kDeskBarScrollViewMinimumHorizontalPaddingDeskButton = 16;

// The corner radius of the desk bar.
constexpr float kDeskBarCornerRadiusOverview = 0.f;
constexpr float kDeskBarCornerRadiusOverviewDeskButton = 16.f;

constexpr int kDeskBarScrollButtonWidth = 36;

// The duration of scrolling one page.
constexpr base::TimeDelta kDeskBarScrollDuration = base::Milliseconds(250);

constexpr int kDeskBarMiniViewsY = 16;

// Spacing between mini views.
constexpr int kDeskBarMiniViewsSpacing = 12;

// Spacing between zero state default desk button and new desk button.
constexpr int kDeskBarZeroStateButtonSpacing = 8;

// The local Y coordinate of the zero state desk buttons.
constexpr int kDeskBarZeroStateY = 6;

constexpr int kDeskBarDeskIconButtonAndLabelSpacing = 8;

// For desk button desk bar, the spacing between shelf and desk bar.
constexpr int kDeskBarShelfAndBarSpacing = 8;

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_CONSTANTS_H_
