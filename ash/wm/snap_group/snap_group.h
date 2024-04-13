// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_H_

#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class ScopedOverviewHideWindows;

// Takes over snap group management after the creation in `SplitViewController`.
// Observes window and window state changes. Implements the
// `LayoutDividerController` interface to allow synchronized resizing of the
// windows within the group. The creation will eventually be done in
// `SnapGroupController` after the major window layout architecture is complete.
class SnapGroup : public aura::WindowObserver,
                  public WindowStateObserver,
                  public LayoutDividerController,
                  public display::DisplayObserver {
 public:
  SnapGroup(aura::Window* window1, aura::Window* window2);
  SnapGroup(const SnapGroup&) = delete;
  SnapGroup& operator=(const SnapGroup&) = delete;
  ~SnapGroup() override;

  aura::Window* window1() const { return window1_; }
  aura::Window* window2() const { return window2_; }
  SplitViewDivider* snap_group_divider() { return &snap_group_divider_; }

  // Gets the window snapped at `snap_type`.
  const aura::Window* GetWindowOfSnapViewType(SnapViewType snap_type) const;

  void ShowDivider();
  void HideDivider();

  // Returns true if snap group is configured in a vertical split-screen layout.
  // Returns false otherwise.
  bool IsSnapGroupLayoutHorizontal();

  // Unified helper to handle mouse/touch events received from
  // `ToplevelWindowEventHandler` to hide `snap_group_divider_` when either of
  // the windows becomes unsnapped.
  void OnLocatedEvent(ui::LocatedEvent* event);

  // Returns the topmost window in the snap group.
  aura::Window* GetTopMostWindowInGroup() const;

  // Minimizes the windows in the snap group.
  void MinimizeWindows();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;

  // WindowStateObserver:
  void OnPreWindowStateTypeChange(WindowState* window_state,
                                  chromeos::WindowStateType old_type) override;

  // LayoutDividerController:
  aura::Window* GetRootWindow() override;
  void StartResizeWithDivider(const gfx::Point& location_in_screen) override;
  void UpdateResizeWithDivider(const gfx::Point& location_in_screen) override;
  bool EndResizeWithDivider(const gfx::Point& location_in_screen) override;
  void OnResizeEnding() override;
  void OnResizeEnded() override;
  void SwapWindows() override;
  gfx::Rect GetSnappedWindowBoundsInScreen(
      SnapPosition snap_position,
      aura::Window* window_for_minimum_size,
      float snap_ratio,
      bool account_for_divider_width) const override;
  SnapPosition GetPositionOfSnappedWindow(
      const aura::Window* window) const override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

 private:
  friend class SnapGroupController;

  // Observes the windows that are added in the `this`.
  void StartObservingWindows();

  // Stops observing the windows when the `this` gets destructed.
  void StopObservingWindows();

  // Updates the bounds of windows in `this`. 'account_for_divider_width'
  // determines whether to adjust the snapped windows' bounds to accommodate the
  // divider.
  void UpdateGroupWindowsBounds(bool account_for_divider_width);

  // Updates the bounds of the given snapped window.
  // - Adjusts window bounds to accommodate the divider if
  // 'account_for_divider_width' is true.
  // - Prioritizes 'snap_ratio' (predetermined snap ratio for the snapped
  // window) over the snap ratio retrieved from the window's state if set.
  void UpdateSnappedWindowBounds(aura::Window* window,
                                 bool account_for_divider_width,
                                 std::optional<float> snap_ratio);

  // Adjusts snapped windows and divider bounds to match the given
  // `primary_snap_ratio`.
  void ApplyPrimarySnapRatio(float primary_snap_ratio);

  // Hides scoped windows in a snap group in partial overview, restores their
  // visibility when partial overview ends.
  void OnOverviewModeStarting();
  void OnOverviewModeEnding();

  // True while the snap group is being moved to another display.
  bool is_moving_display_ = false;

  // Within a snap group, the divider appears as a widget positioned between the
  // two snapped windows. It serves a dual purpose: signifying the group
  // connection and enabling simultaneous resizing of both windows. In terms of
  // stacking order, `snap_group_divider_` is the bottom-most transient child of
  // the top-most window of the two windows.
  SplitViewDivider snap_group_divider_;

  std::unique_ptr<ScopedOverviewHideWindows> hide_windows_in_partial_overview_;

  // The primary snapped window in the group.
  raw_ptr<aura::Window> window1_;

  // The secondary snapped window in the group.
  raw_ptr<aura::Window> window2_;

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_H_
