// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_METRICS_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_METRICS_H_

namespace ash {

// See WindowDragEndEventType at tools/metrics/histograms/enums.xml
enum class WindowDragEndEventType {
  kEndsWithNormalComplete = 0,
  kEndsWithRevert = 1,
  kEndsWithFling = 2,
  kMaxValue = kEndsWithFling,
};

void RecordWindowDragEndTypeHistogram(WindowDragEndEventType type);

// See AppWindowDragEndWindowState at tools/metrics/histograms/enums.xml
enum class AppWindowDragEndWindowState {
  kBackToMaximizedOrFullscreen = 0,
  kDraggedIntoOverview = 1,
  kDraggedIntoSplitView = 2,
  kMaxValue = kDraggedIntoSplitView,
};

void RecordAppDragEndWindowStateHistogram(AppWindowDragEndWindowState state);

// See TabDragType at tools/metrics/histograms/enums.xml
enum class TabDragType {
  kDragSourceWindow = 0,
  kDragTabOutOfWindow = 1,
  kMaxValue = kDragTabOutOfWindow,
};

void RecordTabDragTypeHistogram(TabDragType type);

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_DRAG_METRICS_H_
