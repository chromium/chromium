// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_constants.h"

namespace ash {
namespace scrollable_shelf_constants {

// The definitions of the histogram names for the scrollable shelf dragging
// metrics.
const char kScrollDraggingTabletLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.TabletMode.LauncherVisible";
const char kScrollDraggingTabletLauncherVisibleMaxLatencyHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.TabletMode."
    "LauncherVisible";
const char kScrollDraggingTabletLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.TabletMode.LauncherHidden";
const char kScrollDraggingTabletLauncherHiddenMaxLatencyHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.TabletMode."
    "LauncherHidden";
const char kScrollDraggingClamshellLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.ClamshellMode.LauncherVisible";
const char kScrollDraggingClamshellLauncherVisibleMaxLatencyHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.ClamshellMode."
    "LauncherVisible";
const char kScrollDraggingClamshellLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.ClamshellMode.LauncherHidden";
const char kScrollDraggingClamshellLauncherHiddenMaxLatencyHistogram[] =
    "Apps.ScrollableShelf.Drag.PresentationTime.MaxLatency.ClamshellMode."
    "LauncherHidden";

// The definitions of the histogram names for the scrollable shelf animation
// smoothness metrics.
const char kAnimationSmoothnessHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness";
const char kAnimationSmoothnessTabletLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.TabletMode.LauncherVisible";
const char kAnimationSmoothnessTabletLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.TabletMode.LauncherHidden";
const char kAnimationSmoothnessClamshellLauncherVisibleHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.ClamshellMode.LauncherVisible";
const char kAnimationSmoothnessClamshellLauncherHiddenHistogram[] =
    "Apps.ScrollableShelf.AnimationSmoothness.ClamshellMode.LauncherHidden";

}  // namespace scrollable_shelf_constants
}  // namespace ash
