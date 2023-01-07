// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_drag_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace ash {

constexpr char kWindowDragEndEventTypeHistogram[] =
    "Tablet.WindowDrag.DragEndEventType";
constexpr char kAppDragEndWindowStateHistogram[] =
    "Tablet.AppDrag.EndWindowState";
constexpr char kTabDragTypeHistogram[] = "Tablet.TabDrag.DragType";

void RecordWindowDragEndTypeHistogram(WindowDragEndEventType type) {
  UMA_HISTOGRAM_ENUMERATION(kWindowDragEndEventTypeHistogram, type);
}

void RecordAppDragEndWindowStateHistogram(AppWindowDragEndWindowState state) {
  UMA_HISTOGRAM_ENUMERATION(kAppDragEndWindowStateHistogram, state);
}

void RecordTabDragTypeHistogram(TabDragType type) {
  UMA_HISTOGRAM_ENUMERATION(kTabDragTypeHistogram, type);
}

}  // namespace ash
