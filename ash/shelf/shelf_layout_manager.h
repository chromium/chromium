// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_LAYOUT_MANAGER_H_
#define ASH_SHELF_SHELF_LAYOUT_MANAGER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/session/session_observer.h"
#include "ash/shelf/shelf.h"
#include "ash/shell_observer.h"
#include "ash/wallpaper/wallpaper_controller_observer.h"
#include "ash/wm/lock_state_observer.h"
#include "ash/wm/wm_snap_to_pixel_layout_manager.h"
#include "ash/wm/workspace/workspace_types.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ui {
class ImplicitAnimationObserver;
class MouseEvent;
}  // namespace ui

namespace ash {

enum class AnimationChangeType;
class PanelLayoutManagerTest;
class Shelf;
class ShelfLayoutManagerObserver;
class ShelfLayoutManagerTest;
class ShelfWidget;

// ShelfLayoutManager is the layout manager responsible for the shelf and
// status widgets. The shelf is given the total available width and told the
// width of the status area. This allows the shelf to draw the background and
// layout to the status area.
// To respond to bounds changes in the status area StatusAreaLayoutManager works
// closely with ShelfLayoutManager.
// On mus, widget bounds management is handled by the window manager.
class ASH_EXPORT ShelfLayoutManager
    : public ShellObserver,
      public ::wm::ActivationChangeObserver,
      public keyboard::KeyboardControllerObserver,
      public LockStateObserver,
      public wm::WmSnapToPixelLayoutManager,
      public display::DisplayObserver,
      public SessionObserver,
      public WallpaperControllerObserver {
 public:
  // The snapping threshold for dragging app list from shelf in tablet mode,
  // measured in DIPs.
  static constexpr int kAppListDragSnapToFullscreenThreshold = 320;

  // The snapping thresholds for dragging app list from shelf in laptop mode,
  // measured in DIPs.
  static constexpr int kAppListDragSnapToClosedThreshold = 144;
  static constexpr int kAppListDragSnapToPeekingThreshold = 561;

  // The velocity the app list must be dragged in order to change the state of
  // the app list for fling event, measured in DIPs/event.
  static constexpr int kAppListDragVelocityThreshold = 6;

  ShelfLayoutManager(ShelfWidget* shelf_widget, Shelf* shelf);
  ~ShelfLayoutManager() override;

  bool updating_bounds() const { return updating_bounds_; }

  // Clears internal data for shutdown process.
  void PrepareForShutdown();
  // Returns whether the shelf and its contents (shelf, status) are visible
  // on the screen.
  bool IsVisible() const;

  // Returns the ideal bounds of the shelf assuming it is visible.
  gfx::Rect GetIdealBounds() const;

  // Returns the preferred size of the shelf for the target visibility state.
  gfx::Size GetPreferredSize();

  // Returns the bounds within the root window not occupied by the shelf nor the
  // virtual keyboard.
  const gfx::Rect& user_work_area_bounds() const {
    return user_work_area_bounds_;
  }

  // Stops any animations and sets the bounds of the shelf and status widgets.
  void LayoutShelfAndUpdateBounds();

  // Stops any animations, sets the bounds of the shelf and status widgets, and
  // changes the work area
  void LayoutShelf();

  // Returns shelf visibility state based on current value of auto hide
  // behavior setting.
  ShelfVisibilityState CalculateShelfVisibility();

  // Updates the visibility state.
  void UpdateVisibilityState();

  // Invoked by the shelf when the auto-hide state may have changed.
  void UpdateAutoHideState();

  // Updates the auto-hide state for mouse events.
  void UpdateAutoHideForMouseEvent(ui::MouseEvent* event, aura::Window* target);

  // Process the gesture events on |target|.
  void ProcessGestureEventOnWindow(ui::GestureEvent* event,
                                   aura::Window* target);

  ShelfVisibilityState visibility_state() const {
    return state_.visibility_state;
  }
  ShelfAutoHideState auto_hide_state() const { return state_.auto_hide_state; }

  int accessibility_panel_height() const { return accessibility_panel_height_; }

  int docked_magnifier_height() const { return docked_magnifier_height_; }

  ShelfWidget* shelf_widget() { return shelf_widget_; }

  // Sets whether any windows overlap the shelf. If a window overlaps the shelf
  // the shelf renders slightly differently.
  void SetWindowOverlapsShelf(bool value);
  bool window_overlaps_shelf() const { return window_overlaps_shelf_; }

  void AddObserver(ShelfLayoutManagerObserver* observer);
  void RemoveObserver(ShelfLayoutManagerObserver* observer);

  // Processes a gesture event and updates the status of the shelf when
  // appropriate. Returns true if the gesture has been handled and it should not
  // be processed any further, false otherwise.
  bool ProcessGestureEvent(const ui::GestureEvent& event_in_screen);

  // Returns true if a maximized or fullscreen window is being dragged from the
  // top of the display or from the caption area. Note currently for this case
  // it's only allowed in tablet mode, not in laptop mode.
  bool IsDraggingWindowFromTopOrCaptionArea() const;

  // Returns whether background blur is enabled.
  bool IsBackgroundBlurEnabled() { return is_background_blur_enabled_; }

  // Returns whether the shelf should show a blurred background. This may
  // return false even if background blur is enabled depending on the session
  // state.
  bool ShouldBlurShelfBackground();

  // Overridden from wm::WmSnapToPixelLayoutManager:
  void OnWindowResized() override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  // Overridden from ShellObserver:
  void OnShelfAutoHideBehaviorChanged(aura::Window* root_window) override;
  void OnPinnedStateChanged(aura::Window* pinned_window) override;
  void OnAppListVisibilityChanged(bool shown,
                                  aura::Window* root_window) override;
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;
  void OnSplitViewModeStarted() override;
  void OnSplitViewModeEnded() override;

  // Overridden from wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // Overridden from keyboard::KeyboardControllerObserver:
  void OnKeyboardAppearanceChanged(
      const keyboard::KeyboardStateDescriptor& state) override;
  void OnKeyboardVisibilityStateChanged(bool is_visible) override;

  // Overridden from LockStateObserver:
  void OnLockStateEvent(LockStateObserver::EventType event) override;

  // Overridden from SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnLoginStatusChanged(LoginStatus loing_status) override;

  // Overridden from WallpaperControllerObserver:
  void OnWallpaperBlurChanged() override;
  void OnFirstWallpaperShown() override;

  // DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // TODO(harrym|oshima): These templates will be moved to a new Shelf class.
  // A helper function for choosing values specific to a shelf alignment.
  template <typename T>
  T SelectValueForShelfAlignment(T bottom, T left, T right) const {
    switch (shelf_->alignment()) {
      case SHELF_ALIGNMENT_BOTTOM:
      case SHELF_ALIGNMENT_BOTTOM_LOCKED:
        return bottom;
      case SHELF_ALIGNMENT_LEFT:
        return left;
      case SHELF_ALIGNMENT_RIGHT:
        return right;
    }
    NOTREACHED();
    return right;
  }

  template <typename T>
  T PrimaryAxisValue(T horizontal, T vertical) const {
    return shelf_->IsHorizontalAlignment() ? horizontal : vertical;
  }

  // Returns how the shelf background should be painted.
  ShelfBackgroundType GetShelfBackgroundType() const;

  // Set the height of the accessibility panel, which takes away space from the
  // available work area from the top of the screen. Used by ChromeVox.
  void SetAccessibilityPanelHeight(int height);

  // Set the height of the Docked Magnifier viewport at the top of the screen,
  // which will reduce the available screen work area similarly to the ChromeVox
  // panel height. The Docked Magnifier appears above the ChromeVox panel.
  void SetDockedMagnifierHeight(int height);

  // Updates the background of the shelf if it has changed.
  void MaybeUpdateShelfBackground(AnimationChangeType change_type);

 private:
  class UpdateShelfObserver;
  friend class PanelLayoutManagerTest;
  friend class ShelfLayoutManagerTest;
  friend class NotificationTrayTest;

  struct TargetBounds {
    TargetBounds();
    ~TargetBounds();

    float opacity;
    float status_opacity;
    gfx::Rect shelf_bounds;            // Bounds of the shelf within the screen
    gfx::Rect shelf_bounds_in_shelf;   // Bounds of the shelf minus status area
    gfx::Rect status_bounds_in_shelf;  // Bounds of status area within shelf
    gfx::Insets work_area_insets;
  };

  struct State {
    State();

    // Returns true when a secondary user is being added to an existing session.
    bool IsAddingSecondaryUser() const;

    bool IsScreenLocked() const;

    // Returns whether the session is in an active state.
    bool IsActiveSessionState() const;

    // Returns true if the two states are considered equal. As
    // |auto_hide_state| only matters if |visibility_state| is
    // |SHELF_AUTO_HIDE|, Equals() ignores the |auto_hide_state| as
    // appropriate.
    bool Equals(const State& other) const;

    ShelfVisibilityState visibility_state;
    ShelfAutoHideState auto_hide_state;
    wm::WorkspaceWindowState window_state;
    // True when the system is in the cancelable, pre-lock screen animation.
    bool pre_lock_screen_animation_active;
    session_manager::SessionState session_state;
  };

  // Sets the visibility of the shelf to |state|.
  void SetState(ShelfVisibilityState visibility_state);

  // Updates the bounds and opacity of the shelf and status widgets.
  // If |observer| is specified, it will be called back when the animations, if
  // any, are complete.
  void UpdateBoundsAndOpacity(const TargetBounds& target_bounds,
                              bool animate,
                              ui::ImplicitAnimationObserver* observer);

  // Stops any animations and progresses them to the end.
  void StopAnimating();

  // Calculates the target bounds assuming visibility of |visible|.
  void CalculateTargetBounds(const State& state, TargetBounds* target_bounds);

  // Updates the target bounds if a gesture-drag is in progress. This is only
  // used by |CalculateTargetBounds()|.
  void UpdateTargetBoundsForGesture(TargetBounds* target_bounds) const;

  // Updates the auto hide state immediately.
  void UpdateAutoHideStateNow();

  // Stops the auto hide timer and clears
  // |mouse_over_shelf_when_auto_hide_timer_started_|.
  void StopAutoHideTimer();

  // Returns the bounds of the shelf on the screen. The returned rect does
  // not include portions of the shelf that extend beyond its own display,
  // as those are not visible to the user.
  gfx::Rect GetVisibleShelfBounds() const;

  // Returns the bounds of an additional region which can trigger showing the
  // shelf. This region exists to make it easier to trigger showing the shelf
  // when the shelf is auto hidden and the shelf is on the boundary between
  // two displays.
  gfx::Rect GetAutoHideShowShelfRegionInScreen() const;

  // Returns true if at least one window is visible.
  bool HasVisibleWindow() const;

  // Returns the AutoHideState. This value is determined from the shelf and
  // tray.
  ShelfAutoHideState CalculateAutoHideState(
      ShelfVisibilityState visibility_state) const;

  // Returns true if |window| is a descendant of the shelf.
  bool IsShelfWindow(aura::Window* window);

  // Returns true if |window| is a descendant of the status area.
  bool IsStatusAreaWindow(aura::Window* window);

  int GetWorkAreaInsets(const State& state, int size) const;

  // Called when the LoginUI changes from visible to invisible.
  void UpdateShelfVisibilityAfterLoginUIChange();

  // Compute |target_bounds| opacity based on gesture and shelf visibility.
  float ComputeTargetOpacity(const State& state);

  // Returns true if there is a fullscreen window open that causes the shelf
  // to be hidden.
  bool IsShelfHiddenForFullscreen() const;

  // Returns true if there is a fullscreen or maximized window open that causes
  // the shelf to be autohidden.
  bool IsShelfAutoHideForFullscreenMaximized() const;

  // Gesture related functions:
  bool StartGestureDrag(const ui::GestureEvent& gesture_in_screen);
  void UpdateGestureDrag(const ui::GestureEvent& gesture_in_screen);
  void CompleteGestureDrag(const ui::GestureEvent& gesture_in_screen);
  void CompleteAppListDrag(const ui::GestureEvent& gesture_in_screen);
  void CancelGestureDrag();
  bool CanStartFullscreenAppListDrag(float scroll_y_hint) const;
  float GetAppListBackgroundOpacityOnShelfOpacity();

  // Returns true if the gesture is swiping up on a hidden shelf or swiping down
  // on a visible shelf; other gestures should not change shelf visibility.
  bool IsSwipingCorrectDirection();

  // Returns true if should change the visibility of the shelf after drag.
  bool ShouldChangeVisibilityAfterDrag(
      const ui::GestureEvent& gesture_in_screen);

  // Updates the mask to limit the content to the non lock screen container.
  // The mask will be removed if the workspace state is either in fullscreen
  // or maximized.
  void UpdateWorkspaceMask(wm::WorkspaceWindowState window_state);

  // True when inside UpdateBoundsAndOpacity() method. Used to prevent calling
  // UpdateBoundsAndOpacity() again from SetChildBounds().
  bool updating_bounds_;

  bool in_shutdown_ = false;

  // True if the last mouse event was a mouse drag.
  bool in_mouse_drag_ = false;

  // Current state.
  State state_;

  ShelfWidget* shelf_widget_;
  Shelf* shelf_;

  // Do any windows overlap the shelf? This is maintained by WorkspaceManager.
  bool window_overlaps_shelf_ = false;

  // Whether the app list is visible. This is maintained by
  // OnAppListVisibilityChanged.
  bool is_app_list_visible_ = false;

  base::OneShotTimer auto_hide_timer_;

  // Whether the mouse was over the shelf when the auto hide timer started.
  // False when neither the auto hide timer nor the timer task are running.
  bool mouse_over_shelf_when_auto_hide_timer_started_ = false;

  base::ObserverList<ShelfLayoutManagerObserver>::Unchecked observers_;

  // The shelf reacts to gesture-drags, and can be set to auto-hide for certain
  // gestures. Swiping up from the shelf in tablet mode can open the
  // fullscreen app list. Some shelf behaviour (e.g. visibility state,
  // background color etc.) are affected by various stages of the drag. The enum
  // keeps track of the present status of the gesture drag.
  enum GestureDragStatus {
    GESTURE_DRAG_NONE,
    GESTURE_DRAG_IN_PROGRESS,
    GESTURE_DRAG_CANCEL_IN_PROGRESS,
    GESTURE_DRAG_COMPLETE_IN_PROGRESS,
    GESTURE_DRAG_APPLIST_IN_PROGRESS,
  };

  GestureDragStatus gesture_drag_status_ = GESTURE_DRAG_NONE;

  // Tracks the amount of the drag. The value is only valid when
  // |gesture_drag_status_| is set to GESTURE_DRAG_IN_PROGRESS.
  float gesture_drag_amount_ = 0.f;

  // Tracks the amount of launcher that above the shelf bottom during dragging.
  float launcher_above_shelf_bottom_amount_ = 0.f;

  // Manage the auto-hide state during the gesture.
  ShelfAutoHideState gesture_drag_auto_hide_state_ = SHELF_AUTO_HIDE_SHOWN;

  // Used to delay updating shelf background.
  UpdateShelfObserver* update_shelf_observer_ = nullptr;

  // The occluded bounds of the keyboard. See
  // ui/keyboard/keyboard_controller_observer.h for details.
  gfx::Rect keyboard_occluded_bounds_;

  // The displaced bounds of the keyboard. See
  // ui/keyboard/keyboard_controller_observer.h for details.
  gfx::Rect keyboard_displaced_bounds_;

  // The bounds within the root window not occupied by the shelf nor the virtual
  // keyboard.
  gfx::Rect user_work_area_bounds_;

  // The height of the accessibility panel at the top of the screen, which
  // needs to be removed from the available work area. Used by ChromeVox.
  int accessibility_panel_height_ = 0;

  // The height of the Docked Magnifier viewport at the top of the screen, which
  // similarly to |accessibility_panel_height_| needs to be removed from the
  // available work area.
  int docked_magnifier_height_ = 0;

  // Whether background blur is enabled.
  const bool is_background_blur_enabled_;

  // The display on which this shelf is shown.
  display::Display display_;

  // The current shelf background. Should not be assigned to directly, use
  // MaybeUpdateShelfBackground() instead.
  ShelfBackgroundType shelf_background_type_ = SHELF_BACKGROUND_OVERLAP;

  // Shelf will become transparent if launcher is opened. Stores the shelf
  // background type before open the launcher when start to drag the launcher
  // from shelf.
  ShelfBackgroundType shelf_background_type_before_drag_ =
      SHELF_BACKGROUND_OVERLAP;

  ScopedSessionObserver scoped_session_observer_{this};
  ScopedObserver<WallpaperController, ShelfLayoutManager>
      wallpaper_controller_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManager);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_LAYOUT_MANAGER_H_
