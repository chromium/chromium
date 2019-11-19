// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_WM_GESTURE_HANDLER_H_
#define ASH_WM_GESTURES_WM_GESTURE_HANDLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/optional.h"

namespace ui {
class ScrollEvent;
}

namespace ash {

// This handles 3-finger touchpad scroll events to enter/exit overview mode and
// move the overview highlight if it is visible. This class also handles
// 4-finger horizontal scrolls to switch desks.
class ASH_EXPORT WmGestureHandler {
 public:
  // The thresholds of performing a wm action with a touchpad three or four
  // finger scroll.
  static constexpr float kVerticalThresholdDp = 300.f;
  static constexpr float kHorizontalThresholdDp = 330.f;

  WmGestureHandler();
  virtual ~WmGestureHandler();

  // Processes a scroll event and may switch desks, start overview or move the
  // overivew highlight. Returns true if the event has been handled and should
  // not be processed further, false otherwise.
  bool ProcessScrollEvent(const ui::ScrollEvent& event);

 private:
  // A struct containing the relevant data during a scroll session.
  struct ScrollData {
    int finger_count = 0;
    float scroll_x = 0.f;
    float scroll_y = 0.f;
  };

  // Called when a scroll is ended. Returns true if the scroll is processed.
  bool EndScroll();

  // Tries to move the overview selector. Returns true if successful. Called in
  // the middle of scrolls and when scrolls have ended.
  bool MoveOverviewSelection(int finger_count, float scroll_x, float scroll_y);

  const bool can_handle_desks_gestures_;

  // Contains the data during a scroll session. Empty is no scroll is underway.
  base::Optional<ScrollData> scroll_data_;

  DISALLOW_COPY_AND_ASSIGN(WmGestureHandler);
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_WM_GESTURE_HANDLER_H_
