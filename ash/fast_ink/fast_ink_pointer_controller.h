// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_FAST_INK_POINTER_CONTROLLER_H_
#define ASH_FAST_INK_FAST_INK_POINTER_CONTROLLER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}

namespace views {
class View;
}

namespace fast_ink {

// Base class for a fast ink based pointer controller. Enables/disables
// the pointer, receives points and passes them off to be rendered.
class FastInkPointerController : public ui::EventHandler {
 public:
  FastInkPointerController();
  ~FastInkPointerController() override;

  bool is_enabled() const { return enabled_; }

  // Enables/disables the pointer. The user still has to press to see
  // the pointer.
  virtual void SetEnabled(bool enabled);

 protected:
  // Whether the controller is ready to start handling a new gesture.
  virtual bool CanStartNewGesture(ui::TouchEvent* event);

 private:
  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;

  // Returns the pointer view.
  virtual views::View* GetPointerView() const = 0;

  // Creates the pointer view.
  virtual void CreatePointerView(base::TimeDelta presentation_delay,
                                 aura::Window* root_window) = 0;

  // Updates the pointer view.
  virtual void UpdatePointerView(ui::TouchEvent* event) = 0;

  // Destroys the pointer view if it exists.
  virtual void DestroyPointerView() = 0;

  // The presentation delay used for pointer location prediction.
  const base::TimeDelta presentation_delay_;

  bool enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(FastInkPointerController);
};

}  // namespace fast_ink

#endif  // ASH_FAST_INK_FAST_INK_POINTER_CONTROLLER_H_
