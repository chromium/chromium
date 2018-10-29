// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTOCLICK_AUTOCLICK_CONTROLLER_H
#define ASH_AUTOCLICK_AUTOCLICK_CONTROLLER_H

#include "ash/ash_export.h"
#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"

namespace base {
class RetainingOneShotTimer;
}  // namespace base

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AutoclickDragEventRewriter;
class AutoclickRingHandler;

// Autoclick is one of the accessibility features. If enabled, two circles will
// animate at the mouse event location and an automatic mouse event event will
// happen after a certain amount of time at that location. The event type is
// determined by SetAutoclickEventType.
class ASH_EXPORT AutoclickController : public ui::EventHandler,
                                       public aura::WindowObserver {
 public:
  AutoclickController();
  ~AutoclickController() override;

  // Set whether autoclicking is enabled.
  void SetEnabled(bool enabled);

  // Returns true if autoclicking is enabled.
  bool IsEnabled() const;

  // Set the time to wait in milliseconds from when the mouse stops moving
  // to when the autoclick event is sent.
  void SetAutoclickDelay(base::TimeDelta delay);

  // Gets the default wait time as a base::TimeDelta object.
  static base::TimeDelta GetDefaultAutoclickDelay();

  // Sets the event type.
  void SetAutoclickEventType(mojom::AutoclickEventType type);

  // Sets whether to revert to a left click after any other event type.
  void set_revert_to_left_click(bool revert_to_left_click) {
    revert_to_left_click_ = revert_to_left_click;
  }

 private:
  void SetTapDownTarget(aura::Window* target);
  void CreateAutoclickRingWidget(const gfx::Point& point_in_screen);
  void UpdateAutoclickRingWidget(views::Widget* widget,
                                 const gfx::Point& point_in_screen);
  void DoAutoclickAction();
  void CancelAutoclickAction();
  void OnActionCompleted();
  void InitClickTimer();
  void UpdateRingWidget(const gfx::Point& mouse_location);

  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override;

  bool enabled_;
  mojom::AutoclickEventType event_type_;
  bool revert_to_left_click_;
  // The target window is observed by AutoclickController for the duration
  // of a autoclick gesture.
  aura::Window* tap_down_target_;
  std::unique_ptr<views::Widget> widget_;
  base::TimeDelta delay_;
  int mouse_event_flags_;
  std::unique_ptr<base::RetainingOneShotTimer> autoclick_timer_;
  // The position in screen coordinates used to determine
  // the distance the mouse has moved.
  gfx::Point anchor_location_;
  std::unique_ptr<AutoclickRingHandler> autoclick_ring_handler_;
  std::unique_ptr<AutoclickDragEventRewriter> drag_event_rewriter_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickController);
};

}  // namespace ash

#endif  // ASH_AUTOCLICK_AUTOCLICK_CONTROLLER_H
