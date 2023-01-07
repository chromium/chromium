// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/wm/core/transient_window_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace aura {
class ScopedWindowTargeter;
}  // namespace aura

namespace ash {

class SplitViewController;

// Split view divider. It passes the mouse/gesture events to SplitViewController
// to resize the left and right windows accordingly. The divider widget should
// always placed above its observed windows to be able to receive events.
class ASH_EXPORT SplitViewDivider : public aura::WindowObserver,
                                    public ::wm::ActivationChangeObserver,
                                    public ::wm::TransientWindowObserver {
 public:
  // The distance to the divider edge in which a touch gesture will be
  // considered as a valid event on the divider.
  static constexpr int kDividerEdgeInsetForTouch = 8;

  explicit SplitViewDivider(SplitViewController* controller);

  SplitViewDivider(const SplitViewDivider&) = delete;
  SplitViewDivider& operator=(const SplitViewDivider&) = delete;

  ~SplitViewDivider() override;

  // static version of GetDividerBoundsInScreen(bool is_dragging) function.
  static gfx::Rect GetDividerBoundsInScreen(
      const gfx::Rect& work_area_bounds_in_screen,
      bool landscape,
      int divider_position,
      bool is_dragging);

  // Do the divider spawning animation that adds a finishing touch to the
  // snapping animation of a window.
  void DoSpawningAnimation(int spawn_position);

  // Updates |divider_widget_|'s bounds.
  void UpdateDividerBounds();

  // Calculates the divider's expected bounds according to the divider's
  // position.
  gfx::Rect GetDividerBoundsInScreen(bool is_dragging);

  void SetAlwaysOnTop(bool on_top);

  // Set adjustability of the divider bar. Unadjustable divider does not receive
  // event and the divider bar view is not visible. When the divider is moved
  // for the virtual keyboard, the divider will be set unadjustable.
  void SetAdjustable(bool adjustable);
  // Get the adjustability of the divider bar.
  bool IsAdjustable() const;

  void AddObservedWindow(aura::Window* window);
  void RemoveObservedWindow(aura::Window* window);

  // Called when a window tab(s) are being dragged around the workspace. The
  // divider should be placed beneath the dragged window during dragging.
  void OnWindowDragStarted();
  void OnWindowDragEnded();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // ::wm::TransientWindowObserver:
  void OnTransientChildAdded(aura::Window* window,
                             aura::Window* transient) override;
  void OnTransientChildRemoved(aura::Window* window,
                               aura::Window* transient) override;

  // Checks if the `window` is observed.
  bool IsWindowObserved(const aura::Window* window) const;

  views::Widget* divider_widget() { return divider_widget_; }

 private:
  void CreateDividerWidget(SplitViewController* controller);

  void StartObservingTransientChild(aura::Window* transient);
  void StopObservingTransientChild(aura::Window* transient);

  SplitViewController* controller_;

  // The window targeter that is installed on the always on top container window
  // when the split view mode is active. It deletes itself when the split view
  // mode is ended. Upon destruction, it restores the previous window targeter
  // (if any) on the always on top container window.
  std::unique_ptr<aura::ScopedWindowTargeter> split_view_window_targeter_;

  // Split view divider widget. It's a black bar stretching from one edge of the
  // screen to the other, containing a small white drag bar in the middle. As
  // the user presses on it and drag it to left or right, the left and right
  // window will be resized accordingly.
  views::Widget* divider_widget_ = nullptr;

  // If true there is a window whose tabs are currently being dragged around.
  bool is_dragging_window_ = false;

  // Tracks observed windows.
  aura::Window::Windows observed_windows_;

  // The content view of the divider.
  views::View* divider_view_ = nullptr;

  // Tracks observed transient windows.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      transient_windows_observations_{this};
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_H_
