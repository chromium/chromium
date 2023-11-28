// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_WM_GESTURE_HANDLER_H_
#define ASH_WM_GESTURES_WM_GESTURE_HANDLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "components/prefs/pref_registry_simple.h"

namespace ui {
class ScrollEvent;
}

namespace ash {

// This handles the following interactions:
//   - 3-finger touchpad scroll events to enter/exit overview mode and move the
//   overview focus ring if it is visible.
//   - 4-finger horizontal scrolls to switch desks.
class ASH_EXPORT WmGestureHandler {
 public:
  // The thresholds of performing a wm action with a touchpad three or four
  // finger scroll.
  static constexpr float kVerticalThresholdDp = 300.f;
  static constexpr float kHorizontalThresholdDp = 330.f;

  // The amount in trackpad units the fingers must move in a direction before a
  // continuous gesture animation is started. This is to minimize accidental
  // scrolls.
  static constexpr int kContinuousGestureMoveThresholdDp = 5;

  // The amount that a user must have scrolled, after ending a vertical
  // continuous gesture, to enter overview mode. Otherwise, animate back to
  // the original state before the gesture began.
  static constexpr int kEnterOverviewModeThresholdDp = kVerticalThresholdDp / 2;

  WmGestureHandler();
  WmGestureHandler(const WmGestureHandler&) = delete;
  WmGestureHandler& operator=(const WmGestureHandler&) = delete;
  virtual ~WmGestureHandler();

  // Processes a scroll event and may switch desks, start overview or move the
  // overview focus ring. Returns true if the event has been handled and should
  // not be processed further, false otherwise.
  bool ProcessScrollEvent(const ui::ScrollEvent& event);

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
    bool horizontal_continuous_gesture_started = false;
    bool vertical_continuous_gesture_started = false;
  };

  // Called by ProcessScrollEvent(). Depending on |finger_count|, may switch
  // desks, start overview or move the overview highlight. Returns true if the
  // event has been handled and should not be processed further, false
  // otherwise. Forwards events to DesksController.
  bool ProcessEventImpl(int finger_count, float delta_x, float delta_y);

  // Called when a scroll is ended. Returns true if the scroll is processed.
  bool EndScroll();

  // Called when a scroll is updated. Returns true if the scroll is processed.
  bool UpdateScrollForContinuousOverviewAnimation();

  // Tries to move the overview selector. Returns true if successful. Called in
  // the middle of scrolls and when scrolls have ended.
  bool MoveOverviewSelection(int finger_count, float scroll_x, float scroll_y);

  // Returns whether or not a given session of overview should horizontally
  // scroll.
  bool ShouldHorizontallyScroll(bool in_session,
                                float scroll_x,
                                float scroll_y);

  // Contains the data during a scroll session. Empty is no scroll is underway.
  std::optional<ScrollData> scroll_data_;
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_WM_GESTURE_HANDLER_H_
