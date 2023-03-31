// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_FAST_INK_POINTER_CONTROLLER_H_
#define ASH_FAST_INK_FAST_INK_POINTER_CONTROLLER_H_

#include <set>

#include "base/time/time.h"
#include "ui/aura/window_tracker.h"
#include "ui/events/event_handler.h"

class PrefChangeRegistrar;

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

// Base class for a fast ink based pointer controller. Enables/disables
// the pointer, receives points and passes them off to be rendered.
class FastInkPointerController : public ui::EventHandler {
 public:
  FastInkPointerController();

  FastInkPointerController(const FastInkPointerController&) = delete;
  FastInkPointerController& operator=(const FastInkPointerController&) = delete;

  ~FastInkPointerController() override;

  bool is_enabled() const { return enabled_; }

  // Enables/disables the pointer. The user still has to press to see
  // the pointer.
  virtual void SetEnabled(bool enabled);

  // Add window that should be excluded from handling events.
  void AddExcludedWindow(aura::Window* window);

 protected:
  // Whether the controller is ready to start handling a new gesture.
  virtual bool CanStartNewGesture(ui::LocatedEvent* event);
  // Whether the event should be processed and stop propagation.
  // Default implementation will catch basic mouse events (e.g. mouse clicking)
  // and touch events (e.g. touch pressing) and stop them from being further
  // dispatched, so derived class should override it if the default behavior is
  // not as expected. See b/191044469 as an example.
  virtual bool ShouldProcessEvent(ui::LocatedEvent* event);

  bool IsEnabledForMouseEvent() const;

  // Return true if the location of the event is in one of the excluded windows.
  bool IsPointerInExcludedWindows(ui::LocatedEvent* event);

 private:
  // Creates new pointer view if `can_start_new_gesture` is true. Otherwise, try
  // to re-use existing one. Ends the current pointer session if the pointer
  // widget is no longer valid. Returns true if there is a pointer view
  // available.
  bool MaybeCreatePointerView(ui::LocatedEvent* event,
                              bool can_start_new_gesture);

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  void OnHasSeenStylusPrefChanged();
  void UpdateEnabledForMouseEvent();

  // Returns the pointer view.
  virtual views::View* GetPointerView() const = 0;

  // Creates the pointer view.
  virtual void CreatePointerView(base::TimeDelta presentation_delay,
                                 aura::Window* root_window) = 0;

  // Updates the pointer view.
  virtual void UpdatePointerView(ui::TouchEvent* event) = 0;
  virtual void UpdatePointerView(ui::MouseEvent* event) {}

  // Destroys the pointer view if it exists.
  virtual void DestroyPointerView() = 0;

  // The presentation delay used for pointer location prediction.
  const base::TimeDelta presentation_delay_;

  bool enabled_ = false;
  bool has_seen_stylus_ = false;

  // Set of touch ids.
  std::set<int> touch_ids_;

  // If the pointer event is in the bound of any of the |excluded_windows_|.
  // Skip processing the event.
  aura::WindowTracker excluded_windows_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_local_;
};

}  // namespace ash

#endif  // ASH_FAST_INK_FAST_INK_POINTER_CONTROLLER_H_
