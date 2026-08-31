// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SCROLLABLE_SHELF_CONSTANTS_H_
#define ASH_SHELF_SCROLLABLE_SHELF_CONSTANTS_H_

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ash {
namespace scrollable_shelf_constants {

// The padding between the arrow button and the end of the scrollable shelf. It
// is applied when the arrow button shows.
constexpr int kArrowButtonEndPadding = 6;

// The padding between the shelf container view and the arrow button (if any).
constexpr int kDistanceToArrowButton = 2;

// The size of the arrow button.
constexpr int kArrowButtonSize = 20;

// The distance between the shelf container view and the end of the scrollable
// shelf when the arrow button shows.
constexpr int kArrowButtonGroupWidth =
    kArrowButtonSize + kArrowButtonEndPadding + kDistanceToArrowButton;

// The gesture fling event with the velocity smaller than the threshold will be
// ignored.
constexpr int kGestureFlingVelocityThreshold = 1000;

// The horizontal size of the tap area of the overflow arrow button.
constexpr int kArrowButtonTapAreaHorizontal = 32;

// The length of the fade in/out zone.
constexpr int kGradientZoneLength = 26;

// The time delay to show a new page of shelf icons.
constexpr base::TimeDelta kShelfPageFlipDelay = base::Milliseconds(500);

// The padding at the two ends of the shelf.
constexpr int kEndPadding = 4;

// A mouse wheel event (including touchpad scrolling) should be ignored if its
// offset on the main axis is smaller than the threshold.
ASH_EXPORT constexpr int kScrollOffsetThreshold = 20;

// Histogram names for the scrollable shelf animation smoothness metrics.
inline constexpr char kAnimationSmoothnessHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness";
inline constexpr char kAnimationSmoothnessTabletLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.TabletMode.LauncherVisible";
inline constexpr char kAnimationSmoothnessTabletLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.TabletMode.LauncherHidden";
inline constexpr char kAnimationSmoothnessClamshellLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.ClamshellMode.LauncherVisible";
inline constexpr char kAnimationSmoothnessClamshellLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.ClamshellMode.LauncherHidden";

// Histogram names for the scrollable shelf dragging metrics.
inline constexpr char kScrollDraggingTabletLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.TabletMode.LauncherVisible";
inline constexpr char
    kScrollDraggingTabletLauncherVisibleMaxLatencyHistogram[] =
        "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.TabletMode."
        "LauncherVisible";
inline constexpr char kScrollDraggingTabletLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.TabletMode.LauncherHidden";
inline constexpr char kScrollDraggingTabletLauncherHiddenMaxLatencyHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.TabletMode."
    "LauncherHidden";
inline constexpr char kScrollDraggingClamshellLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.ClamshellMode.LauncherVisible";
inline constexpr char
    kScrollDraggingClamshellLauncherVisibleMaxLatencyHistogram[] =
        "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.ClamshellMode."
        "LauncherVisible";
inline constexpr char kScrollDraggingClamshellLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.ClamshellMode.LauncherHidden";
inline constexpr char
    kScrollDraggingClamshellLauncherHiddenMaxLatencyHistogram[] =
        "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.ClamshellMode."
        "LauncherHidden";

}  // namespace scrollable_shelf_constants
}  // namespace ash

#endif  // ASH_SHELF_SCROLLABLE_SHELF_CONSTANTS_H_
