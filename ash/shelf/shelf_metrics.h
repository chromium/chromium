// Copyright 2019 The Chromium Authors. All rights reserved.
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

  // New items should be added before to keep this one always the last.
  kMaxInAppShelfGestures = 3,

  kMaxValue = kMaxInAppShelfGestures
};

// States of entering overview mode from home launcher. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class EnterOverviewFromHomeLauncher {
  // Cancel the action to enter overview mode from home launcher.
  kCanceled = 0,

  // Succeed to enter overview mode from home launcher.
  kSuccess = 1,

  // New items should be added before to keep this one always the last.
  kMaxState = 2,

  kMaxValue = kMaxState
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_METRICS_H_
