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
ASH_EXPORT constexpr float kHighlightScreenPrimaryAxisRatio = 0.10;

// The padding between a highlight view and the edge of the screen.
ASH_EXPORT constexpr float kHighlightScreenEdgePaddingDp = 12;

// The amount of inset to be applied on a split view label. Here horizontal and
// vertical apply to the orientation before rotation (if there is rotation).
constexpr int kSplitviewLabelHorizontalInsetDp = 16;
constexpr int kSplitviewLabelVerticalInsetDp = 8;

// The preferred height of a split view label.
constexpr int kSplitviewLabelPreferredHeightDp = 36;

// The amount of round applied to the corners of a split view label.
constexpr int kSplitviewLabelRoundRectRadiusDp = 18;

// The thickness of the `split_view_divider_` when it is not being dragged.
constexpr int kSplitviewDividerShortSideLength =
    chromeos::wm::kSplitviewDividerShortSideLength;

// The thickness of the `split_view_divider_` during dragging.
constexpr int kSplitviewDividerEnlargedShortSideLength = 16;

// The time duration for the window transformation animations.
constexpr auto kSplitviewWindowTransformDuration = base::Milliseconds(250);

// The time duration for the `split_view_divider_` animations when dragging
// starts and ends.
constexpr auto kSplitviewDividerSelectionStatusChangeDuration =
    base::Milliseconds(250);

// The time duration for the `split_view_divider_` spawning animation.
constexpr auto kSplitviewDividerSpawnDuration = base::Milliseconds(100);

// The delay before the `split_view_divider_` spawning animation.
constexpr auto kSplitviewDividerSpawnDelay = base::Milliseconds(183);

// The one-way bouncing animation duration for the `split_view_divider_` when
// the to-be-snapped window can't fit in the work area. The actual duration when
// used should be doubled to include the "bouncing out and bounding back in"
// process.
constexpr auto kBouncingAnimationOneWayDuration = base::Milliseconds(250);

// The thickness of the `split_view_divider_`'s handler.
constexpr int kSplitviewWhiteBarShortSideLength = 2;

// The length of the `split_view_divider_`'s handler.
constexpr int kSplitviewWhiteBarLongSideLength = 16;

// The corner radius of the `split_view_divider_`'s handler.
constexpr int kSplitviewWhiteBarCornerRadius = 1;

// The radius of the circular handler when the `split_view_divider_` is being
// dragged.
constexpr int kSplitviewWhiteBarRadius = 4;

// The length of the `split_view_divider_`'s handler when it spawns.
constexpr int kSplitviewWhiteBarSpawnLongSideLength = 2;

// The distance from the `split_view_divider_` to where its handler spawns.
constexpr int kSplitviewWhiteBarSpawnUnsignedOffset = 2;

// The opacity of the highlight area.
constexpr float kHighlightOpacity = 0.25f;

// In portrait mode split view, if the caret in the bottom window is less than
// `kMinCaretKeyboardDist` dip above the upper bounds of the virtual keyboard,
// then we push up the bottom window above the virtual keyboard to avoid the
// input field being occluded by the virtual keyboard. The upper bounds of the
// bottom window after being pushed up cannot exceeds 1 -
// `kMinDividerPositionRatio` of screen height.
constexpr int kMinCaretKeyboardDist = 16;
constexpr float kMinDividerPositionRatio = 0.15f;

// Extra insets used to increase the hit bounds of the split view divider to
// make it easier to handle located event.
constexpr int kSplitViewDividerExtraInset = 8;

// Corner radius for the expanded menu that shows on toggling the kebab button.
constexpr int kExpandedMenuRoundedCornerRadius = 20;

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_CONSTANTS_H_
