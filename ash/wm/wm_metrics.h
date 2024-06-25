// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_METRICS_H_
#define ASH_WM_WM_METRICS_H_

#include <ostream>

#include "ash/ash_export.h"

namespace ash {

// Used to record different ways to snap a window. Note this should be kept in
// sync with `WindowSnapActionSource` enum in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class WindowSnapActionSource {
  // Default value for any snap action actions that's not covered below.
  kNotSpecified,
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
  kSnapGroupWindowUpdate,
  kTest,
  kLacrosSnapButtonOrWindowLayoutMenu,
  kSnapBySwapWindowsInSnapGroup,
  kMaxValue = kSnapBySwapWindowsInSnapGroup,
};

ASH_EXPORT std::ostream& operator<<(std::ostream& out,
                                    WindowSnapActionSource source);

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

// Enum histogram metric for the number of times a window drag results in a
// window split compared to other outcomes. Enum values from
// `ash::WindowSplitter::DragType`.
constexpr char kWindowSplittingDragTypeHistogramName[] =
    "Ash.Wm.WindowSplitting.DragType";

// Enum histogram metric for the window region where a window is split.
// Enum values from `ash::WindowSplitter::SplitRegion`.
constexpr char kWindowSplittingSplitRegionHistogramName[] =
    "Ash.Wm.WindowSplitting.SplitRegion";

// Time histogram metric for the time duration a user spends dragging when a
// window is split.
constexpr char kWindowSplittingDragDurationPerSplitHistogramName[] =
    "Ash.Wm.WindowSplitting.DragDuration.PerSplit";

// Time histogram metric for the time duration a user spends dragging when a
// window is not split.
constexpr char kWindowSplittingDragDurationPerNoSplitHistogramName[] =
    "Ash.Wm.WindowSplitting.DragDuration.PerNoSplit";

// Count histogram metric for the number of times the preview is shown when a
// window is split.
constexpr char kWindowSplittingPreviewsShownCountPerSplitDragHistogramName[] =
    "Ash.Wm.WindowSplitting.PreviewsShownCount.PerSplit";

// Count histogram metric for the number of times the preview is shown when a
// window is not split.
constexpr char kWindowSplittingPreviewsShownCountPerNoSplitDragHistogramName[] =
    "Ash.Wm.WindowSplitting.PreviewsShownCount.PerNoSplit";

}  // namespace ash

#endif  // ASH_WM_WM_METRICS_H_
