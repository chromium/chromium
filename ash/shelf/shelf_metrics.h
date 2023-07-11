// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_METRICS_H_
#define ASH_SHELF_SHELF_METRICS_H_

namespace ash {

// The name of the histogram which records the usage of gestures on the in-app
// hotseat.
constexpr char kHotseatGestureHistogramName[] = "Ash.HotseatGesture";

// The name of the histogram which records the usage of gestures to enter
// overview mode from home screen.
constexpr char kEnterOverviewHistogramName[] = "Ash.SwipeHomeToOverviewGesture";

// The name of the histogram which records the result of handling the drag on
// window from shelf.
constexpr char kHandleDragWindowFromShelfHistogramName[] =
    "Ash.WindowDragFromShelfResult";

// The name of the histogram which records when the user deliberately hides the
// desk button in the shelf.
constexpr char kDeskButtonHiddenHistogramName[] =
    "Ash.Desks.DeskButton.HiddenByUser";

// Types of gestures supported by the in-app hotseat. These values are persisted
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class InAppShelfGestures {
  // Swipe up to show the hotseat.
  kSwipeUpToShow = 0,

  // Swipe down to hide the hotseat.
  kSwipeDownToHide = 1,

  // Fling up to show the home screen.
  kFlingUpToShowHomeScreen = 2,

  // Hotseat hidden due to interacting outside of the shelf/hotseat.
  kHotseatHiddenDueToInteractionOutsideOfShelf = 3,

  // New items should be added before to keep this one always the last.
  kMaxInAppShelfGestures = 4,

  kMaxValue = kMaxInAppShelfGestures
};

// States of entering overview mode from home launcher. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class EnterOverviewFromHomeLauncher {
  // Cancel the action to enter overview mode from home launcher.
  kCanceled = 0,

  // Succeed to enter overview mode from home launcher.
  kOverview = 1,

  // The gesture was detected as a swipe to the home screen initial state.
  kBack = 2,

  // New items should be added before to keep this one always the last.
  kMaxState = 3,

  kMaxValue = kMaxState
};

// Results of handling the drag on window from shelf. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class ShelfWindowDragResult {
  // Goes to home screen after drag.
  kGoToHomeScreen = 0,

  // Dragged window restored to its original bounds after drag.
  kRestoreToOriginalBounds = 1,

  // Dragged window is dropped to overview after drag.
  kGoToOverviewMode = 2,

  // Enter splitview mode after drag.
  kGoToSplitviewMode = 3,

  // Drag is canceled instead of ending normally.
  kDragCanceled = 4,

  kMaxValue = kDragCanceled
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_METRICS_H_
