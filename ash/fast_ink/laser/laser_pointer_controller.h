// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_LASER_LASER_POINTER_CONTROLLER_H_
#define ASH_FAST_INK_LASER_LASER_POINTER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/fast_ink/fast_ink_pointer_controller.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

class LaserPointerView;

// A checked observer which receives notification of changes to the Laser
// Pointer activation state.
class ASH_EXPORT LaserPointerObserver : public base::CheckedObserver {
 public:
  virtual void OnLaserPointerStateChanged(bool enabled) {}
};

// Controller for the laser pointer functionality. Enables/disables laser
// pointer as well as receives points and passes them off to be rendered.
class ASH_EXPORT LaserPointerController : public FastInkPointerController,
                                          public aura::WindowObserver {
 public:
  LaserPointerController();

  LaserPointerController(const LaserPointerController&) = delete;
  LaserPointerController& operator=(const LaserPointerController&) = delete;

  ~LaserPointerController() override;

  // Adds/removes the specified |observer|.
  void AddObserver(LaserPointerObserver* observer);
  void RemoveObserver(LaserPointerObserver* observer);

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroyed(aura::Window* window) override;

  // fast_ink::FastInkPointerController:
  void SetEnabled(bool enabled) override;

 private:
  friend class LaserPointerControllerTestApi;
  class ScopedLockedHiddenCursor;

  // fast_ink::FastInkPointerController:
  views::View* GetPointerView() const override;
  void CreatePointerView(base::TimeDelta presentation_delay,
                         aura::Window* root_window) override;
  void UpdatePointerView(ui::TouchEvent* event) override;
  void UpdatePointerView(ui::MouseEvent* event) override;
  void DestroyPointerView() override;
  void ResetPointerView() override;
  bool ShouldProcessEvent(ui::LocatedEvent* event) override;
  bool IsPointerInExcludedRegion(ui::LocatedEvent* event) const override;

  void NotifyStateChanged(bool enabled);

  // Called when the fade out animation is complete.
  void OnFadeOutComplete();

  // Helper method to fade out the laser pointer view or stop the destroy timer.
  void HandlePointerReleaseEvent(LaserPointerView* laser_pointer_view,
                                 bool is_released);

  // Returns the content view of the |laser_pointer_view_widget_| as a
  // LaserPointerView*.
  LaserPointerView* GetLaserPointerView() const;

  // |laser_pointer_view_widget_| will only hold an instance when the laser
  // pointer is enabled and activated (pressed or dragged).
  views::UniqueWidgetPtr laser_pointer_view_widget_;
  base::ObserverList<LaserPointerObserver> observers_;

  base::OneShotTimer keep_alive_timer_;

  std::unique_ptr<ScopedLockedHiddenCursor> scoped_locked_hidden_cursor_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      root_window_observation_{this};
};

}  // namespace ash

#endif  // ASH_FAST_INK_LASER_LASER_POINTER_CONTROLLER_H_
