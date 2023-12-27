// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_EVENT_FILTER_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_EVENT_FILTER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
class GestureEvent;
class KeyEvent;
class MouseEvent;
class ScrollEvent;
}  // namespace ui

namespace ash {

// Created by WindowCycleController when cycling through windows. Eats all key
// events and stops cycling when the necessary key sequence is encountered.
// Also allows users to cycle using right/left keys.
class ASH_EXPORT WindowCycleEventFilter : public ui::EventHandler {
 public:
  // The threshold of performing an action with a touchpad or mouse wheel
  // scroll.
  static constexpr float kHorizontalThresholdDp = 330.f;

  WindowCycleEventFilter();

  WindowCycleEventFilter(const WindowCycleEventFilter&) = delete;
  WindowCycleEventFilter& operator=(const WindowCycleEventFilter&) = delete;

  ~WindowCycleEventFilter() override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // A struct containing the relevant data during a scroll session.
  struct ScrollData {
    int finger_count = 0;

    // Values are cumulative (ex. |scroll_x| is the total x distance moved
    // since the scroll began.
    float scroll_x = 0.f;
    float scroll_y = 0.f;
  };

  class AltReleaseHandler : public ui::EventHandler {
   public:
    AltReleaseHandler();

    AltReleaseHandler(const AltReleaseHandler&) = delete;
    AltReleaseHandler& operator=(const AltReleaseHandler&) = delete;

    ~AltReleaseHandler() override;

    // ui::EventHandler:
    void OnKeyEvent(ui::KeyEvent* event) override;
  };

  // Depending on the values of |event| either repeatedly cycle through windows,
  // stop repeatedly cycling through windows, or cycle once.
  void HandleTriggerKey(ui::KeyEvent* event);

  // Returns whether the window cycle should repeatedly cycle in the
  // direction given by |event|.
  bool ShouldRepeatKey(ui::KeyEvent* event) const;

  // Given |event|, determine if the user has used their mouse, i.e. moved or
  // clicked.
  void SetHasUserUsedMouse(ui::MouseEvent* event);

  // Depending on the properties of |event|, may cycle the window cycle list or
  // complete cycling.
  void ProcessMouseEvent(ui::MouseEvent* event);

  // Depending on the properties of |event|, may continuously scroll the window
  // cycle list, move the cycle view's focus ring or complete cycling.
  void ProcessGestureEvent(ui::GestureEvent* event);

  // Called by ProcessMouseEvent() and OnScrollEvent(). May cycle the window
  // cycle list. Returns true if the event has been handled and should not be
  // processed further, false otherwise.
  bool ProcessEventImpl(int finger_count, float delta_x, float delta_y);

  // Based on the given scroll data, determine whether we should cycle the
  // window cycle list. Return true if we do cycle the window cycle list,
  // otherwise return false.
  bool CycleWindowCycleList(int finger_count, float scroll_x, float scroll_y);

  // Returns the cycling direction the window cycle should cycle depending on
  // the combination of keys being pressed.
  WindowCycleController::WindowCyclingDirection GetWindowCyclingDirection(
      ui::KeyEvent* event) const;

  // Returns the navigation direction to move the focus to.
  WindowCycleController::KeyboardNavDirection GetKeyboardNavDirection(
      ui::KeyEvent* event) const;

  // When the user holds Alt+Tab, this timer is used to send repeated
  // cycle commands to WindowCycleController. Note this is not accomplished
  // by marking the Alt+Tab accelerator as "repeatable" in the accelerator
  // table because we wish to control the repeat interval.
  base::RepeatingTimer repeat_timer_;

  AltReleaseHandler alt_release_handler_;

  // Stores the initial mouse coordinates. Used to determine whether
  // |has_user_used_mouse_| when this handles mouse events.
  gfx::Point initial_mouse_location_;

  // Bool for tracking whether the user has used their mouse. If this is false,
  // mouse events should be filtered. This is to prevent the initial mouse
  // position from triggering window cycle items' mouse event handlers despite a
  // user not moving their mouse. Should be set to true when a user moves their
  // mouse enough or clicks/drags/mousewheel scrolls.
  // See crbug.com/114375.
  bool has_user_used_mouse_ = false;

  // Stores the current scroll session data. If it does not exist, there is no
  // active scroll session.
  std::optional<ScrollData> scroll_data_;

  // When a user taps on a preview item it should move the focus ring to it.
  // However, the focus ring should not move if the user is scrolling. Store
  // |tapped_window_| on tap events and determine whether this is a tap or
  // scroll with subsequent events.
  raw_ptr<aura::Window> tapped_window_ = nullptr;

  // Tracks whether the user is touch scrolling the window cycle list.
  bool touch_scrolling_ = false;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_EVENT_FILTER_H_
