// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_util.h"

#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/root_window_controller.h"
#include "base/check.h"
#include "base/notreached.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace capture_mode_util {

gfx::Point GetLocationForFineTunePosition(const gfx::Rect& rect,
                                          FineTunePosition position) {
  switch (position) {
    case FineTunePosition::kTopLeft:
      return rect.origin();
    case FineTunePosition::kTopCenter:
      return rect.top_center();
    case FineTunePosition::kTopRight:
      return rect.top_right();
    case FineTunePosition::kRightCenter:
      return rect.right_center();
    case FineTunePosition::kBottomRight:
      return rect.bottom_right();
    case FineTunePosition::kBottomCenter:
      return rect.bottom_center();
    case FineTunePosition::kBottomLeft:
      return rect.bottom_left();
    case FineTunePosition::kLeftCenter:
      return rect.left_center();
    default:
      break;
  }

  NOTREACHED();
  return gfx::Point();
}

bool IsCornerFineTunePosition(FineTunePosition position) {
  switch (position) {
    case FineTunePosition::kTopLeft:
    case FineTunePosition::kTopRight:
    case FineTunePosition::kBottomRight:
    case FineTunePosition::kBottomLeft:
      return true;
    default:
      break;
  }
  return false;
}

bool ShouldHideDragAffordance(FineTunePosition position) {
  // Do not show affordance circles when repositioning the whole region or if
  // resizing on a corner.
  return position == FineTunePosition::kCenter ||
         IsCornerFineTunePosition(position);
}

void SetStopRecordingButtonVisibility(aura::Window* root, bool visible) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  // Recording can end when a display being fullscreen-captured gets removed, in
  // this case, we don't need to hide the button.
  if (root->is_destroying()) {
    DCHECK(!visible);
    return;
  }

  // Can be null while shutting down.
  auto* root_window_controller = RootWindowController::ForWindow(root);
  if (!root_window_controller)
    return;

  auto* stop_recording_button = root_window_controller->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  DCHECK(stop_recording_button);
  stop_recording_button->SetVisiblePreferred(visible);
}

}  // namespace capture_mode_util

}  // namespace ash
