// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/transient_window_observer.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace ash {

class LayoutDividerController;
class SnapGroupController;
class SplitViewDividerView;

// Observes the windows in the split view and controls the stacking orders among
// the split view divider and its observed windows. The divider widget should
// always be placed above its observed windows to be able to receive events
// unless it's being dragged.
class ASH_EXPORT SplitViewDivider : public aura::WindowObserver,
                                    public wm::TransientWindowObserver,
                                    public display::DisplayObserver {
 public:
  // The split view resize behavior in tablet mode. The normal mode resizes
  // windows on drag events. In the fast mode, windows are instead moved. A
  // single drag "session" may involve both modes.
  enum class TabletResizeMode {
    kNormal,
    kFast,
  };

  explicit SplitViewDivider(LayoutDividerController* controller);
  SplitViewDivider(const SplitViewDivider&) = delete;
  SplitViewDivider& operator=(const SplitViewDivider&) = delete;
  ~SplitViewDivider() override;

  // static
  // Returns the divider bounds in screen where `divider_position` is in the
  // divider's root window's bounds.
  static gfx::Rect GetDividerBoundsInScreen(
      const gfx::Rect& work_area_bounds_in_screen,
      bool landscape,
      int divider_position,
      bool is_dragging);

  views::Widget* divider_widget() { return divider_widget_; }

  int divider_position() const { return divider_position_; }

  bool target_visibility() const { return target_visibility_; }

  bool is_resizing_with_divider() const { return is_resizing_with_divider_; }

  // Does not consider any order of `observed_windows_`. Clients of the divider
  // are responsible for maintaining the order themselves.
  const aura::Window::Windows& observed_windows() const {
    return observed_windows_;
  }
  const gfx::Point previous_event_location() const {
    return previous_event_location_;
  }

  // Returns the divider widget's native window, or nullptr if none exists.
  aura::Window* GetDividerWindow();

  // Returns true if the divider widget is created.
  bool HasDividerWidget() const;

  bool IsDividerWidgetVisible() const;

  // Updates the divider's target visibility.
  void SetVisible(bool visible);

  // Sets the divider's position in root window bounds, ensuring it meets the
  // minimum window size requirement.
  void SetDividerPosition(int divider_position);

  // Updates divider position while resizing, keeping it within allowed range.
  void UpdateDividerPosition(const gfx::Point& location_in_screen);

  // Returns the root window of this.
  aura::Window* GetRootWindow() const;

  // Resizing functions used when resizing with `split_view_divider_` in the
  // tablet split view mode or clamshell mode if `kSnapGroup` is enabled.
  void StartResizeWithDivider(const gfx::Point& location_in_screen);
  void ResizeWithDivider(const gfx::Point& location_in_screen);
  void EndResizeWithDivider(const gfx::Point& location_in_screen);

  // Finalizes and cleans up divider dragging/animating. Called when the divider
  // snapping animation completes or is interrupted or totally skipped, or by
  // external events (split view ending, tablet mode ending, etc.).
  void CleanUpWindowResizing();

  // Updates `divider_widget_`'s bounds.
  void UpdateDividerBounds();

  // Calculates the divider's expected bounds according to the divider's
  // position.
  gfx::Rect GetDividerBoundsInScreen(bool is_dragging);

  // Provides visual feedback by adjusting `divider_widget_` bounds in response
  // to user hover or drag interactions (enlarged on interaction, thin default).
  void EnlargeOrShrinkDivider(bool should_enlarge);

  // Sets the adjustability of the divider bar. Unadjustable divider does not
  // receive event and the divider bar view is not visible. When the divider is
  // moved for the virtual keyboard, the divider will be set unadjustable.
  void SetAdjustable(bool adjustable);

  // Returns true if the divider bar is adjustable.
  bool IsAdjustable() const;

  void MaybeAddObservedWindow(aura::Window* window);
  void MaybeRemoveObservedWindow(aura::Window* window);

  // Called by the LayoutDividerController on a keyboard bounds change, where
  // `work_area` is the total work area and `y` is the vertical position of the
  // bottom window.
  void OnKeyboardOccludedBoundsChangedInPortrait(const gfx::Rect& work_area,
                                                 int y);

  // Called when a window tab(s) are being dragged around the workspace. The
  // divider should be placed beneath the dragged window during dragging and be
  // placed above the dragged window when drag is completed.
  void OnWindowDragStarted(aura::Window* dragged_window);
  void OnWindowDragEnded();

  // Calls the delegate to swap the windows.
  void SwapWindows();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // wm::TransientWindowObserver:
  void OnTransientChildAdded(aura::Window* window,
                             aura::Window* transient) override;
  void OnTransientChildRemoved(aura::Window* window,
                               aura::Window* transient) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  SplitViewDividerView* divider_view_for_testing() { return divider_view_; }

 private:
  class SplitViewDividerWidget;

  // Refreshes the divider's state by creating or closing the divider widget if
  // needed, and updating its visibility, bounds, and stacking order as needed.
  // If `observed_windows_changed` is true, this will refresh the divider
  // position and stacking order.
  void RefreshDividerState(bool observed_windows_changed);

  void CreateDividerWidget(int divider_position);
  void CloseDividerWidget();

  // Returns the `TargetVisibility()` of the `divider_widget_`,  which directly
  // assesses the window's target visibility, regardless of the visibility of
  // its parent's layer.
  bool GetActualTargetVisibility() const;

  // Refreshes the stacking order of the `divider_widget_` to be right on top of
  // the `observed_windows_` and reparents the split view divider to be on the
  // same parent container of the above window of the `observed_windows_` while
  // not dragging. The `divider_widget` will be temporarily stacked below the
  // window being dragged and reparented if the window being dragged has
  // different parent with the divider widget native window.
  void RefreshStackingOrder();

  void StartObservingTransientChild(aura::Window* transient);
  void StopObservingTransientChild(aura::Window* transient);

  // Gets the expected end drag position for `window` depending on current
  // screen orientation and split divider position.
  gfx::Point GetEndDragLocationInScreen(aura::Window* window) const;

  // Finalizes and cleans up after stopping dragging the divider bar to resize
  // snapped windows.
  void FinishWindowResizing();

  const raw_ptr<LayoutDividerController> controller_;

  // The distance between the origin of `divider_widget_` and the origin
  // of the current display's work area in screen coordinates, which essentially
  // makes it relative to the divider widget's root window's work area.
  //     |<---     divider_position_    --->|
  //     ---------------------------------------------------------------
  //     |                                  | |                        |
  //     |        primary window            | |   secondary window     |
  //     |                                  | |                        |
  //     ---------------------------------------------------------------
  // Initialized as -1 before `divider_widget_` is created and shown.
  int divider_position_ = -1;

  // True if the divider widget should be shown, false otherwise.
  bool target_visibility_ = false;

  // Split view divider widget. It's a black bar stretching from one edge of the
  // screen to the other, containing a small white drag bar in the middle. As
  // the user presses on it and drag it to left or right, the left and right
  // window will be resized accordingly.
  raw_ptr<views::Widget> divider_widget_ = nullptr;

  // The contents view of the `divider_widget_`.
  raw_ptr<SplitViewDividerView> divider_view_ = nullptr;

  // This variable indicates the dragging state and records the window being
  // dragged which will be used to refresh the stacking order of the
  // `divider_widget_` to be stacked below the `dragged_window_`.
  raw_ptr<aura::Window> dragged_window_ = nullptr;

  // The window(s) observed by the divider which will be updated upon adding or
  // removing window. Note this does not guarantee any order about which of the
  // `observed_windows_` is primary or secondary snapped.
  aura::Window::Windows observed_windows_;

  // If true, skip refreshing the divider state. This is used to avoid recursive
  // updates when updating the divider state.
  bool is_refreshing_state_ = false;

  // If true, skip the stacking order update. This is used to avoid recursive
  // update when updating the stacking order.
  bool is_refreshing_stacking_order_ = false;

  // Tracks observed transient windows.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      transient_windows_observations_{this};

  // True when the divider is being dragged (not during its snap animation).
  bool is_resizing_with_divider_ = false;

  // The location of the previous mouse/gesture event in screen coordinates.
  gfx::Point previous_event_location_;

  // True *while* a resize event is being processed.
  bool processing_resize_event_ = false;

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_H_
