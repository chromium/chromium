// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_WM_GESTURE_HANDLER_H_
#define ASH_WM_GESTURES_WM_GESTURE_HANDLER_H_

#include "ash/ash_export.h"
#include "base/optional.h"
#include "components/prefs/pref_registry_simple.h"

namespace ui {
class MouseEvent;
class ScrollEvent;
}

namespace ash {

// TODO(chinsenj): Consider renaming this to WmEventHandler and moving to parent
// directory since this now handles mouse wheel events.

// This handles the following interactions:
//   - 3-finger touchpad scroll events to enter/exit overview mode and move the
//   overview highlight if it is visible.
//   - 4-finger horizontal scrolls to switch desks.
//
// This handles the following interactions if the InteractiveWindowCycleList
// flag is enabled. TODO(chinsenj): Merge these comments when the flag is
// removed.
//   - 3-finger horizontal touchpad scroll events to cycle the window cycle
//   list.
//   - 2-finger horizontal touchpad scroll events to cycle the window cycle
//   list.
//   - Mouse wheel events to cycle the window cycle list.
class ASH_EXPORT WmGestureHandler {
 public:
  // The thresholds of performing a wm action with a touchpad three or four
  // finger scroll.
  static constexpr float kVerticalThresholdDp = 300.f;
  static constexpr float kHorizontalThresholdDp = 330.f;

  WmGestureHandler();
  WmGestureHandler(const WmGestureHandler&) = delete;
  WmGestureHandler& operator=(const WmGestureHandler&) = delete;
  virtual ~WmGestureHandler();

  // Processes a mouse wheel event and may cycle the window cycle list. Returns
  // true if the event has been handled and should not be processed further,
  // false otherwise.
  bool ProcessWheelEvent(const ui::MouseEvent& event);

  // Processes a scroll event and may switch desks, start overview, move the
  // overview highlight or cycle the window cycle list. Returns true if
  // the event has been handled and should not be processed further, false
  // otherwise.
  bool ProcessScrollEvent(const ui::ScrollEvent& event);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // A struct containing the relevant data during a scroll session.
  struct ScrollData {
    int finger_count = 0;

    // Values are cumulative (ex. |scroll_x| is the total x distance moved
    // since the scroll began.
    float scroll_x = 0.f;
    float scroll_y = 0.f;

    // Continuous gestures need to first pass a threshold before we update the
    // UI. We still update this struct before that happens.
    bool continuous_gesture_started = false;
  };

  // Called by ProcessWheelEvent() and ProcessScrollEvent(). Depending on
  // |finger_count|, may switch desks, start overview, move the overview
  // highlight or cycle the window cycle list. Returns true if the
  // event has been handled and should not be processed further, false
  // otherwise. Forwards events to DesksController if
  // |is_enhanced_desk_animations_| is true.
  bool ProcessEventImpl(int finger_count, float delta_x, float delta_y);

  // Called when a scroll is ended. Returns true if the scroll is processed.
  bool EndScroll();

  // Tries to move the overview selector. Returns true if successful. Called in
  // the middle of scrolls and when scrolls have ended.
  bool MoveOverviewSelection(int finger_count, float scroll_x, float scroll_y);

  // Tries to cycle the window cycle list. Returns true if successful.
  // Called in the middle of scrolls and when scrolls have ended.
  bool CycleWindowCycleList(int finger_count, float scroll_x, float scroll_y);

  // Returns whether or not a given session of overview/window cycle list should
  // horizontally scroll.
  bool ShouldHorizontallyScroll(bool in_session,
                                float scroll_x,
                                float scroll_y);

  // Contains the data during a scroll session. Empty is no scroll is underway.
  base::Optional<ScrollData> scroll_data_;

  // True when the enhanced desk animations feature is enabled.
  const bool is_enhanced_desk_animations_;
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_WM_GESTURE_HANDLER_H_
