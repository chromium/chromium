// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MAGNIFIER_PARTIAL_MAGNIFICATION_CONTROLLER_H_
#define ASH_ACCESSIBILITY_MAGNIFIER_PARTIAL_MAGNIFICATION_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class Window;
}

namespace ui {
class LocatedEvent;
struct PointerDetails;
}  // namespace ui

namespace ash {

class MagnifierGlass;

// Controls the partial screen magnifier, which is a small area of the screen
// which is zoomed in.  The zoomed area follows the mouse cursor when enabled.
class ASH_EXPORT PartialMagnificationController : public ui::EventHandler {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the partial magnification enabled state changes.
    virtual void OnPartialMagnificationStateChanged(bool enabled) = 0;
  };

  PartialMagnificationController();
  ~PartialMagnificationController() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Turns the partial screen magnifier feature on or off. Turning the magnifier
  // on does not imply that it will be displayed; the magnifier is only
  // displayed when it is both enabled and active.
  void SetEnabled(bool enabled);

  // Switch PartialMagnified RootWindow to |new_root_window|. This does
  // following:
  //  - Remove the magnifier from the current root window.
  //  - Create a magnifier in the new root_window |new_root_window|.
  //  - Switch the target window from current window to |new_root_window|.
  void SwitchTargetRootWindowIfNeeded(aura::Window* new_root_window);

  void set_allow_mouse_following(bool enabled) {
    allow_mouse_following_ = enabled;
  }

  bool is_enabled() const { return is_enabled_; }

 private:
  friend class PartialMagnificationControllerTestApi;

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Enables or disables the actual magnifier window.
  void SetActive(bool active);

  // Contains common logic between OnMouseEvent and OnTouchEvent.
  void OnLocatedEvent(ui::LocatedEvent* event,
                      const ui::PointerDetails& pointer_details);

  bool is_enabled_ = false;
  bool is_active_ = false;

  // If enabled, allows the magnifier glass to follow the mouse without the need
  // to pressing and holding the mouse.
  bool allow_mouse_following_ = false;

  std::unique_ptr<MagnifierGlass> magnifier_glass_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(PartialMagnificationController);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MAGNIFIER_PARTIAL_MAGNIFICATION_CONTROLLER_H_
