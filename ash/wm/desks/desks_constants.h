// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_CONSTANTS_H_
#define ASH_WM_DESKS_DESKS_CONSTANTS_H_

#include "ash/constants/ash_features.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

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

inline constexpr int kExpandedDeskBarHeight =
    kDeskBarNonPreviewAllocatedHeight + 16;

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

// For desk button desk bar, number of desk activation shortcuts
// supported.
constexpr int kDeskBarMaxDeskShortcut = 8;

// For desk button desk bar, time in milliseconds it takes from when the desk
// button desk bar enter event was received and successfully processed to when
// the next frame is shown to the user.
inline constexpr char kDeskBarEnterPresentationHistogram[] =
    "Ash.Desks.DeskButton.DeskBar.Enter.PresentationTime";

// For desk button desk bar, time in milliseconds it takes from when the desk
// button desk bar exit event was received and successfully processed to when
// the next frame is shown to the user.
inline constexpr char kDeskBarExitPresentationHistogram[] =
    "Ash.Desks.DeskButton.DeskBar.Exit.PresentationTime";

// For desk button desk bar, max latency of entering/exiting the desk bar.
inline constexpr base::TimeDelta kDeskBarEnterExitPresentationMaxLatency =
    base::Seconds(2);

// Constant values used by the desk button.
inline constexpr int kDeskButtonCornerRadius = 28;
inline constexpr int kDeskButtonFocusRingHaloInset = -3;
inline constexpr int kDeskButtonHeightVertical = 36;
inline constexpr int kDeskButtonWidthVertical = 36;
inline constexpr int kDeskButtonHeightHorizontal = 28;
inline constexpr int kDeskButtonWidthHorizontalZeroNoAvatar = 28;
inline constexpr int kDeskButtonWidthHorizontalZeroWithAvatar = 48;
inline constexpr int kDeskButtonWidthHorizontalExpandedNoAvatar = 128;
inline constexpr int kDeskButtonWidthHorizontalExpandedWithAvatar = 144;
inline constexpr int kDeskButtonChildSpacingHorizontalZero = 4;
inline constexpr int kDeskButtonChildSpacingHorizontalExpanded = 8;
inline constexpr gfx::Size kDeskButtonAvatarSize = gfx::Size(20, 20);
inline constexpr gfx::Insets kDeskButtonInsetVerticalNoAvatar =
    gfx::Insets::TLBR(4, 4, 4, 4);
inline constexpr gfx::Insets kDeskButtonInsetHorizontalZeroWithAvatar =
    gfx::Insets::TLBR(0, 4, 0, 4);
inline constexpr gfx::Insets kDeskButtonInsetHorizontalZeroNoAvatar =
    gfx::Insets::TLBR(0, 0, 0, 0);
inline constexpr gfx::Insets kDeskButtonInsetHorizontalExpandedWithAvatar =
    gfx::Insets::TLBR(0, 4, 0, 10);
inline constexpr gfx::Insets kDeskButtonInsetHorizontalExpandedNoAvatar =
    gfx::Insets::TLBR(0, 10, 0, 10);

// Constant values used by the desk switch button.
inline constexpr int kDeskButtonSwitchButtonCornerRadius = 4;
inline constexpr int kDeskButtonSwitchButtonWidth = 20;
inline constexpr int kDeskButtonSwitchButtonHeightVertical = 36;
inline constexpr int kDeskButtonSwitchButtonHeightHorizontal = 28;
inline constexpr int kDeskButtonSwitchButtonSpacing = 2;
inline constexpr int kDeskButtonSwitchButtonFocusRingHaloInset = 0;

// Constant values used by the desk button container.
inline constexpr int kDeskButtonContainerCornerRadius = 36;
inline constexpr gfx::Insets kDeskButtonContainerInsetsVertical =
    gfx::Insets::TLBR(0, 0, 0, 0);
inline constexpr int kDeskButtonContainerHeightVertical =
    kDeskButtonHeightVertical + kDeskButtonContainerInsetsVertical.height();
inline constexpr int kDeskButtonContainerWidthVertical =
    kDeskButtonHeightVertical + kDeskButtonContainerInsetsVertical.width();
inline constexpr gfx::Insets kDeskButtonContainerInsetsHorizontal =
    gfx::Insets::TLBR(4, 4, 4, 4);
inline constexpr int kDeskButtonContainerHeightHorizontal =
    kDeskButtonHeightHorizontal + kDeskButtonContainerInsetsHorizontal.height();
inline constexpr int kDeskButtonContainerChildSpacingHorizontal = 4;
inline constexpr int kDeskButtonContainerWidthHorizontalZeroNoAvatar = 82;
inline constexpr int kDeskButtonContainerWidthHorizontalExpandedNoAvatar = 182;
inline constexpr int kDeskButtonContainerWidthHorizontalZeroWithAvatar = 102;
inline constexpr int kDeskButtonContainerWidthHorizontalExpandedWithAvatar =
    198;

// Constant values used by the desk button widget.
inline constexpr gfx::Insets kDeskButtonWidgetInsetsVertical =
    gfx::Insets::TLBR(3, 6, 14, 6);
inline constexpr gfx::Insets kDeskButtonWidgetInsetsHorizontal =
    gfx::Insets::TLBR(6, 4, 6, 16);
}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_CONSTANTS_H_
