// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_METRICS_H_
#define ASH_WM_WM_METRICS_H_

namespace ash {

// Used to record different ways to snap a window. Note this should be kept in
// sync with WindowSnapActionSource enum in tools/metrics/histograms/enums.xml.
enum class WindowSnapActionSource {
  kNotSpecified,  // Default value. This can include any actions that's not
                  // covered below. Examples can be: swap action in split view,
                  //
  kDragWindowToEdgeToSnap,
  kLongPressCaptionButtonToSnap,
  kKeyboardShortcutToSnap,
  kDragOrSelectOverviewWindowToSnap,
  kLongPressOverviewButtonToSnap,
  kDragUpFromShelfToSnap,
  kDragDownFromTopToSnap,
  kDragTabToSnap,
  kAutoSnapInSplitView,
  kSnapByWindowStateRestore,
  kSnapByWindowLayoutMenu,
  kSnapByFullRestoreOrDeskTemplateOrSavedDesk,
  kSnapByClamshellTabletTransition,
  kSnapByDeskOrSessionChange,
  kMaxValue = kSnapByDeskOrSessionChange,
};

// Used to save histogram metrics about how the user initiates window snapping.
constexpr char kWindowSnapActionSourceHistogram[] =
    "Ash.Wm.WindowSnapActionSource2";

// Used to record the user action on initiating the multi-window resizer.
constexpr char kMultiWindowResizerShow[] = "MultiWindowResizerShow";

// Used to record the user action on initiating the multi-window resizer when
// two windows are snapped.
constexpr char kMultiWindowResizerShowTwoWindowsSnapped[] =
    "MultiWindowResizerShow_TwoWindowsSnapped";

// Used to record the user action on clicking on the multi-window resizer.
constexpr char kMultiWindowResizerClick[] = "MultiWindowResizerClick";

// Used to record the user action on clicking on the multi-window resizer when
// two windows are snapped.
constexpr char kMultiWindowResizerClickTwoWindowsSnapped[] =
    "MultiWindowResizerClick_TwoWindowsSnapped";

// Used to record the histogram metrics on initiating the multi-window resizer.
constexpr char kMultiWindowResizerShowHistogramName[] =
    "Ash.Wm.MultiWindowResizerShow";

// Used to record the histogram metrics on initiating the multi-window resizer
// when two windows are snapped.
constexpr char kMultiWindowResizerShowTwoWindowsSnappedHistogramName[] =
    "Ash.Wm.MultiWindowResizerShowTwoWindowsSnapped";

// Used to record the histogram metrics on clicking on the multi-window resizer.
constexpr char kMultiWindowResizerClickHistogramName[] =
    "Ash.Wm.MultiWindowResizerClick";

// Used to record the histogram metrics on clicking on the multi-window resizer
// when two windows are snapped.
constexpr char kMultiWindowResizerClickTwoWindowsSnappedHistogramName[] =
    "Ash.Wm.MultiWindowResizerClickTwoWindowsSnapped";

}  // namespace ash

#endif  // ASH_WM_WM_METRICS_H_
