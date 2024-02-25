// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MAGNIFIER_PARTIAL_MAGNIFIER_CONTROLLER_H_
#define ASH_ACCESSIBILITY_MAGNIFIER_PARTIAL_MAGNIFIER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
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
class ASH_EXPORT PartialMagnifierController : public ui::EventHandler {
 public:
  PartialMagnifierController();
  PartialMagnifierController(const PartialMagnifierController&) = delete;
  PartialMagnifierController& operator=(const PartialMagnifierController&) =
      delete;
  ~PartialMagnifierController() override;

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

 private:
  friend class PartialMagnifierControllerTestApi;

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;

  // Enables or disables the actual magnifier window.
  void SetActive(bool active);

  // Contains common logic between OnMouseEvent and OnTouchEvent.
  void OnLocatedEvent(ui::LocatedEvent* event,
                      const ui::PointerDetails& pointer_details);

  bool is_enabled_ = false;
  bool is_active_ = false;

  raw_ptr<aura::Window, DanglingUntriaged> current_root_window_ = nullptr;

  std::unique_ptr<MagnifierGlass> magnifier_glass_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MAGNIFIER_PARTIAL_MAGNIFIER_CONTROLLER_H_
