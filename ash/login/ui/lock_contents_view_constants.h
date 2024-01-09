// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_CONSTANTS_H_
#define ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_CONSTANTS_H_

namespace ash {

namespace {

// Any non-zero value used for separator height. Makes debugging easier; this
// should not affect visual appearance.
constexpr int kNonEmptyHeightDp = 30;

// Horizontal distance between two users in the low density layout.
constexpr int kLowDensityDistanceBetweenUsersInLandscapeDp = 118;
constexpr int kLowDensityDistanceBetweenUsersInPortraitDp = 32;

constexpr int kMediaControlsSpacingThreshold = 1280;
constexpr int kMediaControlsSmallSpaceFactor = 3;
constexpr int kMediaControlsLargeSpaceFactor = 5;

// Margin left of the auth user in the medium density layout.
constexpr int kMediumDensityMarginLeftOfAuthUserLandscapeDp = 98;
constexpr int kMediumDensityMarginLeftOfAuthUserPortraitDp = 0;

// Horizontal distance between the auth user and the medium density user row.
constexpr int kMediumDensityDistanceBetweenAuthUserAndUsersLandscapeDp = 220;
constexpr int kMediumDensityDistanceBetweenAuthUserAndUsersPortraitDp = 84;

// Spacing between the bottom status indicator and the shelf.
constexpr int kBottomStatusIndicatorBottomMarginDp = 16;

// Spacing between icon and text in the bottom status indicator.
constexpr int kBottomStatusIndicatorChildSpacingDp = 8;

// Spacing between child of LoginBaseBubbleView.
constexpr int kBubbleBetweenChildSpacingDp = 16;

// Border radius of the rounded bubble.
constexpr int kBubbleBorderRadius = 8;

// Width of the user adding screen
constexpr int kUserAddingScreenIndicatorWidth = 512;

// Distance from the top of the user view to the user icon.
constexpr int kDistanceFromTopOfBigUserViewToUserIconDp = 24;

// Distance from the bottom of the user adding screen indicator to the user
// icon.
constexpr int kDistanceFromBottomOfIndicatorToUserIconDp =
    96 - kDistanceFromTopOfBigUserViewToUserIconDp;

// Min distance from the top of the screen to the top of the user adding screen
// indicator.
constexpr int kMinDistanceFromTopOfScreenToIndicatorDp = 8;

// Padding around the login screen bubble view.
constexpr int kBubblePaddingDp = 16;

// Size of the tooltip view info icon.
constexpr int kInfoIconSizeDp = 20;

// Horizontal and vertical padding of login tooltip view.
constexpr int kHorizontalPaddingLoginTooltipViewDp = 8;
constexpr int kVerticalPaddingLoginTooltipViewDp = 8;

}  // namespace

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_CONSTANTS_H_
