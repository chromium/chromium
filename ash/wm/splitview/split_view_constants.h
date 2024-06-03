// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_CONSTANTS_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_CONSTANTS_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "chromeos/ui/wm/constants.h"

namespace ash {

// The ratio between a highlight view's primary axis, and the screens
// primary axis.
ASH_EXPORT inline constexpr float kHighlightScreenPrimaryAxisRatio = 0.10;

// The padding between a highlight view and the edge of the screen.
ASH_EXPORT inline constexpr float kHighlightScreenEdgePaddingDp = 12;

// The amount of inset to be applied on a split view label. Here horizontal and
// vertical apply to the orientation before rotation (if there is rotation).
inline constexpr int kSplitviewLabelHorizontalInsetDp = 16;
inline constexpr int kSplitviewLabelVerticalInsetDp = 8;

// App cannot snap toast id.
inline constexpr char kAppCannotSnapToastId[] = "split_view_app_cannot_snap";

// The preferred height of a split view label.
inline constexpr int kSplitviewLabelPreferredHeightDp = 36;

// The amount of round applied to the corners of a split view label.
inline constexpr int kSplitviewLabelRoundRectRadiusDp = 18;

// The thickness of the `split_view_divider_` when it is not being dragged.
inline constexpr int kSplitviewDividerShortSideLength =
    chromeos::wm::kSplitviewDividerShortSideLength;

// The thickness of the `split_view_divider_` during dragging.
inline constexpr int kSplitviewDividerEnlargedShortSideLength = 8;

// The divider handler's default / enlarged short side length.
inline constexpr int kDividerHandlerShortSideLength = 2;
inline constexpr int kDividerHandlerEnlargedShortSideLength = 4;

// The divider handler's default / enlarged long side length.
inline constexpr int kDividerHandlerLongSideLength = 16;
inline constexpr int kDividerHandlerEnlargedLongSideLength = 48;

// The time duration for the window transformation animations.
inline constexpr auto kSplitviewWindowTransformDuration =
    base::Milliseconds(250);

// The one-way bouncing animation duration for the `split_view_divider_` when
// the to-be-snapped window can't fit in the work area. The actual duration when
// used should be doubled to include the "bouncing out and bounding back in"
// process.
inline constexpr auto kBouncingAnimationOneWayDuration =
    base::Milliseconds(250);

// The opacity of the highlight area.
inline constexpr float kHighlightOpacity = 0.25f;

// In portrait mode split view, if the caret in the bottom window is less than
// `kMinCaretKeyboardDist` dip above the upper bounds of the virtual keyboard,
// then we push up the bottom window above the virtual keyboard to avoid the
// input field being occluded by the virtual keyboard. The upper bounds of the
// bottom window after being pushed up cannot exceeds 1 -
// `kMinDividerPositionRatio` of screen height.
inline constexpr int kMinCaretKeyboardDist = 16;
inline constexpr float kMinDividerPositionRatio = 0.15f;

// Extra insets used to increase the hit bounds of the split view divider to
// make it easier to handle located event.
inline constexpr int kSplitViewDividerExtraInset = 8;

// The distance that the divider and windows are resized by when the divider is
// resized via the keyboard.
inline constexpr int kSplitViewDividerResizeDistance = 10;

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_CONSTANTS_H_
