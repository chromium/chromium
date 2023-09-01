// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_H_

#include "ash/ash_export.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/core/transient_window_observer.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace ash {

class SplitViewController;
class SplitViewDividerView;

// Observes the windows in the split view and controls the stacking orders among
// the split view divider and its observed windows. The divider widget should
// always be placed above its observed windows to be able to receive events
// unless it's being dragged.
class ASH_EXPORT SplitViewDivider : public aura::WindowObserver,
                                    public ::wm::TransientWindowObserver {
 public:
  // The split view resize behavior in tablet mode. The normal mode resizes
  // windows on drag events. In the fast mode, windows are instead moved. A
  // single drag "session" may involve both modes.
  enum class TabletResizeMode {
    kNormal,
    kFast,
  };

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

  views::Widget* divider_widget() { return divider_widget_; }

  bool is_resizing_with_divider() const { return is_resizing_with_divider_; }

  // Used by SplitViewController to immediately stop resizing in case of
  // external events (split view ending, tablet mode ending, etc.).
  // TODO(sophiewen): See if we can call `EndResizeWithDivider()` instead.
  void set_is_resizing_with_divider(bool is_resizing_with_divider) {
    is_resizing_with_divider_ = is_resizing_with_divider;
  }

  // Resizing functions used when resizing with `split_view_divider_` in the
  // tablet split view mode or clamshell mode if `kSnapGroup` is enabled.
  void StartResizeWithDivider(const gfx::Point& location_in_screen);
  void ResizeWithDivider(const gfx::Point& location_in_screen);
  void EndResizeWithDivider(const gfx::Point& location_in_screen);

  // Do the divider spawning animation that adds a finishing touch to the
  // snapping animation of a window.
  void DoSpawningAnimation(int spawn_position);

  // Updates `divider_widget_`'s bounds.
  void UpdateDividerBounds();

  // Calculates the divider's expected bounds according to the divider's
  // position.
  gfx::Rect GetDividerBoundsInScreen(bool is_dragging);

  // Sets the adjustability of the divider bar. Unadjustable divider does not
  // receive event and the divider bar view is not visible. When the divider is
  // moved for the virtual keyboard, the divider will be set unadjustable.
  void SetAdjustable(bool adjustable);

  // Returns true if the divider bar is adjustable.
  bool IsAdjustable() const;

  void AddObservedWindow(aura::Window* window);
  void RemoveObservedWindow(aura::Window* window);

  // Called when a window tab(s) are being dragged around the workspace. The
  // divider should be placed beneath the dragged window during dragging and be
  // placed above the dragged window when drag is completed.
  void OnWindowDragStarted(aura::Window* dragged_window);
  void OnWindowDragEnded();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;

  // ::wm::TransientWindowObserver:
  void OnTransientChildAdded(aura::Window* window,
                             aura::Window* transient) override;
  void OnTransientChildRemoved(aura::Window* window,
                               aura::Window* transient) override;

  SplitViewDividerView* divider_view_for_testing() { return divider_view_; }
  const aura::Window::Windows& observed_windows_for_testing() const {
    return observed_windows_;
  }

 private:
  friend class SplitViewController;

  void CreateDividerWidget(SplitViewController* controller);

  // Refreshes the stacking order of the `divider_widget_` to be right on top of
  // the `observed_windows_` and reparents the split view divider to be on the
  // same parent container of the above window of the `observed_windows_` while
  // not dragging. The `divider_widget` will be temporarily stacked below the
  // window being dragged and reparented if the window being dragged has
  // different parent with the divider widget native window.
  void RefreshStackingOrder();

  void StartObservingTransientChild(aura::Window* transient);
  void StopObservingTransientChild(aura::Window* transient);

  raw_ptr<SplitViewController, ExperimentalAsh> controller_;

  // Split view divider widget. It's a black bar stretching from one edge of the
  // screen to the other, containing a small white drag bar in the middle. As
  // the user presses on it and drag it to left or right, the left and right
  // window will be resized accordingly.
  raw_ptr<views::Widget, ExperimentalAsh> divider_widget_ = nullptr;

  // The contents view of the `divider_widget_`.
  raw_ptr<SplitViewDividerView, ExperimentalAsh> divider_view_ = nullptr;

  // This variable indicates the dragging state and records the window being
  // dragged which will be used to refresh the stacking order of the
  // `divider_widget_` to be stacked below the `dragged_window_`.
  raw_ptr<aura::Window, ExperimentalAsh> dragged_window_ = nullptr;

  // The window(s) observed by the divider which will be updated upon adding or
  // removing window.
  aura::Window::Windows observed_windows_;

  // If true, skip the stacking order update. This is used to avoid recursive
  // update when updating the stacking order.
  bool pause_update_ = false;

  // Tracks observed transient windows.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      transient_windows_observations_{this};

  // True when the divider is being dragged (not during its snap animation).
  bool is_resizing_with_divider_ = false;

  // The location of the previous mouse/gesture event in screen coordinates.
  gfx::Point previous_event_location_;

  // True *while* a resize event is being processed.
  bool processing_resize_event_ = false;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_H_
