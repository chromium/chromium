// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_METRICS_H_
#define ASH_WM_WM_METRICS_H_

namespace ash {

// Used to record different ways to snap a window. Note this should be kept in
// sync with WindowSnapActionSource enum in tools/metrics/histograms/enums.xml.
enum class WindowSnapActionSource {
  kDragWindowToEdgeToSnap,
  kUseCaptionButtonToSnap,
  kKeyboardShortcutToSnap,
  kDragOrSelectOverviewWindowToSnap,
  kLongPressOverviewButtonToSnap,
  kDragUpFromShelfToSnap,
  kDragDownFromTopToSnap,
  kDragTabToSnap,
  kAutoSnapBySplitview,
  kSnapByWindowStateRestore,
  kOthers,  // This can include any actions that's not covered above, e.g.,
            // window snap by full restore, desk template, desk switch or user
            // switch, etc
  kMaxValue = kOthers,
};

// Used to save histogram metrics about how the user initiates window snapping.
constexpr char kWindowSnapActionSourceHistogram[] =
    "Ash.Wm.WindowSnapActionSource";

}  // namespace ash

#endif  // ASH_WM_WM_METRICS_H_
