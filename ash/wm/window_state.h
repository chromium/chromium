// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_STATE_H_
#define ASH_WM_WINDOW_STATE_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/wm/drag_details.h"
#include "ash/wm/multi_display/persistent_window_info.h"
#include "ash/wm/wm_metrics.h"
#include "base/auto_reset.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window_observer.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/gfx/animation/tween.h"

namespace chromeos {
enum class WindowPinType;
enum class WindowStateType;
}  // namespace chromeos

namespace gfx {
class Rect;
}

namespace ash {
class ClientControlledState;
class LockWindowState;
class TabletModeWindowState;
class WindowState;
class WindowStateDelegate;
class WindowStateObserver;
class WMEvent;

// WindowState manages and defines ash specific window state and
// behavior. Ash specific per-window state (such as ones that controls
// window manager behavior) and ash specific window behavior (such as
// maximize, minimize, snap sizing etc) should be added here instead
// of defining separate functions (like |MaximizeWindow(aura::Window*
// window)|) or using aura Window property.
// The WindowState gets created when first accessed by
// |WindowState::Get()|, and deleted when the window is deleted.
// Prefer using this class instead of passing aura::Window* around in
// ash code as this is often what you need to interact with, and
// accessing the window using |window()| is cheap.
class ASH_EXPORT WindowState : public aura::WindowObserver {
 public:
  // A subclass of State class represents one of the window's states
  // that corresponds to chromeos::WindowStateType in Ash environment, e.g.
  // maximized, minimized or side snapped, as subclass.
  // Each subclass defines its own behavior and transition for each WMEvent.
  class State {
   public:
    State() = default;

    State(const State&) = delete;
    State& operator=(const State&) = delete;

    virtual ~State() {}

    // Update WindowState based on |event|.
    virtual void OnWMEvent(WindowState* window_state, const WMEvent* event) = 0;

    virtual chromeos::WindowStateType GetType() const = 0;

    // Gets called when the state object became active and the managed window
    // needs to be adjusted to the State's requirement.
    // The passed |previous_state| may be used to properly implement state
    // transitions such as bound animations from the previous state.
    // Note: This only gets called when the state object gets changed.
    virtual void AttachState(WindowState* window_state,
                             State* previous_state) = 0;

    // Gets called before the state objects gets deactivated / detached from the
    // window, so that it can save the various states it is interested in.
    // Note: This only gets called when the state object gets changed.
    virtual void DetachState(WindowState* window_state) = 0;

    // Called when the window is being destroyed.
    virtual void OnWindowDestroying(WindowState* window_state) {}
  };

  // Type of animation type to be applied when changing bounds locally.
  // TODO(oshima): Use transform animation for snapping.
  enum class BoundsChangeAnimationType {
    // No animation (`SetBoundsDirect()`).
    kNone,
    // Cross fade animation. Copies old layer, and fades it out while fading the
    // new layer in.
    kCrossFade,
    // Custom cross fade animations when floating/unfloating a window.
    kCrossFadeFloat,
    kCrossFadeUnfloat,
    // Bounds animation.
    kAnimate,
    // Bounds animation with zero tween. Updates the bounds once at the end of
    // the animation.
    kAnimateZero,
  };

  // The default duration for an animation between two sets of bounds.
  static constexpr base::TimeDelta kBoundsChangeSlideDuration =
      base::Milliseconds(120);

  // Returns the WindowState for |window|. Creates WindowState if it doesn't
  // exist. The returned value is owned by |window| (you should not delete it).
  static WindowState* Get(aura::Window* window);
  static const WindowState* Get(const aura::Window* window);

  // Returns the WindowState for the active window, null if there is no active
  // window.
  static WindowState* ForActiveWindow();

  WindowState(const WindowState&) = delete;
  WindowState& operator=(const WindowState&) = delete;

  // Call WindowState::Get() to instantiate this class.
  ~WindowState() override;

  aura::Window* window() { return window_; }
  const aura::Window* window() const { return window_; }

  bool is_moving_to_another_display() const {
    return is_moving_to_another_display_;
  }
  void set_is_moving_to_another_display(bool moving) {
    is_moving_to_another_display_ = moving;
  }

  void set_can_update_snap_ratio(bool val) { can_update_snap_ratio_ = val; }

  std::optional<float> snap_ratio() const { return snap_ratio_; }

  std::optional<WindowSnapActionSource> snap_action_source() const {
    return snap_action_source_;
  }

  // True if the window should be unminimized to the restore bounds, as
  // opposed to the window's current bounds. |unminimized_to_restore_bounds_| is
  // reset to the default value after the window is unminimized.
  bool unminimize_to_restore_bounds() const {
    return unminimize_to_restore_bounds_;
  }
  void set_unminimize_to_restore_bounds(bool value) {
    unminimize_to_restore_bounds_ = value;
  }

  // Gets/sets whether the shelf should be autohidden when this window is
  // fullscreen or active.
  // Note: if true, this will override the logic controlled by
  // hide_shelf_when_fullscreen.
  bool autohide_shelf_when_maximized_or_fullscreen() const {
    return autohide_shelf_when_maximized_or_fullscreen_;
  }
  void set_autohide_shelf_when_maximized_or_fullscreen(bool value) {
    autohide_shelf_when_maximized_or_fullscreen_ = value;
  }

  // Gets/Sets the bounds of the window before it was moved by the auto window
  // management. As long as it was not auto-managed, it will return NULL.
  const std::optional<gfx::Rect> pre_auto_manage_window_bounds() {
    return pre_auto_manage_window_bounds_;
  }
  void set_pre_auto_manage_window_bounds(const gfx::Rect& bounds) {
    pre_auto_manage_window_bounds_ = std::make_optional(bounds);
  }

  // Gets/Sets the property that is used on window added to workspace event.
  const std::optional<gfx::Rect> pre_added_to_workspace_window_bounds() {
    return pre_added_to_workspace_window_bounds_;
  }
  void set_pre_added_to_workspace_window_bounds(const gfx::Rect& bounds) {
    pre_added_to_workspace_window_bounds_ = std::make_optional(bounds);
  }

  // Gets the persistent window info that is used on restoring persistent
  // window bounds in multi-displays scenario.
  PersistentWindowInfo* persistent_window_info_of_display_removal() {
    return persistent_window_info_of_display_removal_.get();
  }
  void reset_persistent_window_info_of_display_removal() {
    persistent_window_info_of_display_removal_.reset();
  }

  // Gets the persistent window info that is used to restore persistent
  // window bounds on screen rotation.
  PersistentWindowInfo* persistent_window_info_of_screen_rotation() {
    return persistent_window_info_of_screen_rotation_.get();
  }

  // Whether the window is being dragged.
  bool is_dragged() const { return !!drag_details_; }

  // Whether or not the window's position or size was changed by a user.
  bool bounds_changed_by_user() const { return bounds_changed_by_user_; }

  // True if the window should not adjust the window's bounds when
  // virtual keyboard bounds changes.
  // TODO(oshima): This is hack. Replace this with proper
  // implementation based on EnsureCaretNotInRect.
  bool ignore_keyboard_bounds_change() const {
    return ignore_keyboard_bounds_change_;
  }
  void set_ignore_keyboard_bounds_change(bool ignore_keyboard_bounds_change) {
    ignore_keyboard_bounds_change_ = ignore_keyboard_bounds_change;
  }

  // True if the window bounds can be updated directly using SET_BOUNDS event.
  void set_allow_set_bounds_direct(bool value) {
    allow_set_bounds_direct_ = value;
  }
  bool allow_set_bounds_direct() const { return allow_set_bounds_direct_; }

  // Returns a pointer to DragDetails during drag operations.
  const DragDetails* drag_details() const { return drag_details_.get(); }
  DragDetails* drag_details() { return drag_details_.get(); }

  const std::vector<chromeos::WindowStateType>& window_state_restore_history()
      const {
    return window_state_restore_history_;
  }

  bool HasDelegate() const;
  void SetDelegate(std::unique_ptr<WindowStateDelegate> delegate);

  // Creates PersistentWindowInfo on display removal or display rotation.
  // `for_display_removal` indicates to create
  // `persistent_window_info_of_display_removal_`, otherwise
  // `persistent_window_info_of_screen_rotation_`.
  void CreatePersistentWindowInfo(bool was_landscape_before_rotation,
                                  const gfx::Rect& restore_bounds_in_parent,
                                  bool for_display_removal);

  // Returns the window's current ash state type.
  // Refer to chromeos::WindowStateType definition in wm_types.h as for why Ash
  // has its own state type.
  chromeos::WindowStateType GetStateType() const;

  // Predicates to check window state.
  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsFullscreen() const;
  bool IsSnapped() const;
  bool IsPinned() const;
  bool IsTrustedPinned() const;
  bool IsPip() const;
  bool IsFloated() const;

  // Gets the id of the display to show fullscreen on.
  // Returns kInvalidDisplay if not set.
  int64_t GetFullscreenTargetDisplayId() const;

  // True if the window's state type is chromeos::WindowStateType::kMaximized,
  // chromeos::WindowStateType::kFullscreen or
  // chromeos::WindowStateType::kPinned.
  bool IsMaximizedOrFullscreenOrPinned() const;

  // True if the window's state type is chromeos::WindowStateType::kNormal or
  // chromeos::WindowStateType::kDefault.
  bool IsNormalStateType() const;

  bool IsNormalOrSnapped() const;

  // Returns true if the window is vertical or horizontal maximized. The window
  // is in normal state type with vertical or horizontal axis maximized.
  bool IsVerticalOrHorizontalMaximized() const;

  // Return true if the window is in normal state but not horizontal or vertical
  // maximized.
  bool IsNonVerticalOrHorizontalMaximizedNormalState() const;

  bool IsActive() const;

  // Returns true if the window's location can be controlled by the user.
  bool IsUserPositionable() const;

  // Checks if the window can change its state accordingly.
  bool CanFullscreen() const;
  bool CanMaximize() const;
  bool CanMinimize() const;
  bool CanResize() const;
  // CanSnap() checks if the window can be snapped on the display which
  // currently the window is on, whereas CanSnapOnDisplay() checks the
  // snappability on the given |display|.
  bool CanSnap();
  bool CanSnapOnDisplay(display::Display display) const;
  bool CanActivate() const;

  // Returns true if the window has restore bounds.
  bool HasRestoreBounds() const;

  // These methods use aura::WindowProperty to change the window's state
  // instead of using WMEvent directly. This is to use the same mechanism as
  // what views::Widget is using.
  void Maximize();
  void Minimize();
  void Unminimize();

  void Activate();
  void Deactivate();

  // Set the window state to its previous applicable window state.
  void Restore();

  // Determines whether transitioning from the `previous_state` to the current
  // state counts as restoring.
  bool IsRestoring(chromeos::WindowStateType previous_state) const;

  // Caches, then disables z-ordering state and then stacks |window_| below
  // |window_on_top| if |window_| currently has a special z-order.
  void DisableZOrdering(aura::Window* window_on_top);

  // Restores the z-ordering state that a window might have cached.
  void RestoreZOrdering();

  // Invoked when a WMevent occurs, which drives the internal
  // state machine.
  void OnWMEvent(const WMEvent* event);

  // Sets `bounds` as is and ensure the layer is aligned with pixel boundary.
  // For use in unit tests.
  void SetBoundsDirectForTesting(const gfx::Rect& bounds);

  // TODO(oshima): Try hiding these methods and making them accessible only to
  // state impl. State changes should happen through events (as much
  // as possible).

  // Gets the current bounds in screen DIP coordinates.
  gfx::Rect GetCurrentBoundsInScreen() const;

  // Saves the current bounds to be used as a restore bounds.
  void SaveCurrentBoundsForRestore();

  // Same as |GetRestoreBoundsInScreen| except that it returns the
  // bounds in the parent's coordinates.
  gfx::Rect GetRestoreBoundsInParent() const;

  // Returns the restore bounds property on the window in the virtual screen
  // coordinates. The bounds can be NULL if the bounds property does not
  // exist for the window. The window owns the bounds object.
  gfx::Rect GetRestoreBoundsInScreen() const;

  // Same as |SetRestoreBoundsInScreen| except that the bounds is in the
  // parent's coordinates.
  void SetRestoreBoundsInParent(const gfx::Rect& bounds_in_parent);

  // Sets the restore bounds property on the window in the virtual screen
  // coordinates.  Deletes existing bounds value if exists.
  void SetRestoreBoundsInScreen(const gfx::Rect& bounds_in_screen);

  // Deletes and clears the restore bounds property on the window.
  void ClearRestoreBounds();

  // Shrink window from work_area/vertical maximized state.
  // If window is not vertically shrinkable, return false.
  bool VerticallyShrinkWindow(const gfx::Rect& work_area);

  // Shrink window from work_area/horizontal maximized state.
  // If window is not horizontally shrinkable, return false.
  bool HorizontallyShrinkWindow(const gfx::Rect& work_area);

  // Replace the State object of a window with a state handler which can
  // implement a new window manager type. The passed object will be owned
  // by this object and the returned object will be owned by the caller.
  std::unique_ptr<State> SetStateObject(std::unique_ptr<State> new_state);

  // Updates |snap_ratio_| with the current snapped window to screen ratio.
  // Should be called by snap events and bound events, or when resizing a
  // snapped window.
  void UpdateSnapRatio();

  // Forcefully updates `snap_ratio` based on the given `target_bounds`. You
  // usually should use `UpdateSnapRatio()` instead. This method does not check
  // whether `window()` is in the snapped state, so the caller must be sure that
  // `window()` is to-be-snapped. Use with care.
  void ForceUpdateSnapRatio(const gfx::Rect& target_bounds);

  // Gets/sets whether the shelf should be hidden when this window is
  // fullscreen.
  bool GetHideShelfWhenFullscreen() const;
  void SetHideShelfWhenFullscreen(bool value);

  void AddObserver(WindowStateObserver* observer);
  void RemoveObserver(WindowStateObserver* observer);

  // Whether or not the window's position can be managed by the
  // auto management logic.
  bool GetWindowPositionManaged() const;
  void SetWindowPositionManaged(bool managed);

  // True if the window should be offered a chance to consume special system
  // keys such as brightness, volume, etc. that are usually handled by the
  // shell.
  bool CanConsumeSystemKeys() const;
  void SetCanConsumeSystemKeys(bool can_consume_system_keys);

  // True if the window is in "immersive full screen mode" which is slightly
  // different from the normal fullscreen mode by allowing the user to reveal
  // the top portion of the window through a touch / mouse gesture. It might
  // also allow the shelf to be shown in some situations.
  bool IsInImmersiveFullscreen() const;

  // Sets `bounds_changed_by_user_` to the given value and resets the
  // corresponding variables.
  void SetBoundsChangedByUser(bool bounds_changed_by_user);

  // Creates and takes ownership of a pointer to DragDetails when resizing is
  // active. This should be done before a resizer gets created.
  void CreateDragDetails(const gfx::PointF& point_in_parent,
                         int window_component,
                         ::wm::WindowMoveSource source);

  // Deletes and clears a pointer to DragDetails. This should be done when the
  // resizer gets destroyed.
  void DeleteDragDetails();

  // Sets the currently stored restore bounds and clears the restore bounds.
  void SetAndClearRestoreBounds();

  // Notifies that the drag operation has been started. Optionally returns a
  // presentation time recorder for the drag.
  std::unique_ptr<PresentationTimeRecorder> OnDragStarted(int window_component);

  // Notifies that the drag operation has been either completed or reverted.
  // |location| is the last position of the pointer device used to drag.
  void OnCompleteDrag(const gfx::PointF& location);
  void OnRevertDrag(const gfx::PointF& location);

  // Notifies that the window lost the activation.
  void OnActivationLost();

  // Returns the Display that this WindowState is on.
  display::Display GetDisplay() const;

  // Returns the WindowStateType to restore to from the current window state.
  // TODO(aluh): Rename to GetWindowStateTypeForRestore() for clarity.
  chromeos::WindowStateType GetRestoreWindowState() const;

  // Called when `window_` is dragged to maximized to track if it's a
  // mis-triggered drag to maximize behavior.
  void TrackDragToMaximizeBehavior();

  // Allows for caller to prevent property changes within scope.
  base::AutoReset<bool> GetScopedIgnorePropertyChange();

  // Returns true if `current_state_` is `ClientControlledState`.
  // A client-controlled window behaves in a manner distinct from other windows
  // (e.g., windows backed by `DefaultState`). So when making modifications to a
  // window management component, be careful about the following considerations.
  // 1. Client dominance
  //  All window state/bounds changes (excluding “direct methods”) made to a
  //  client-controlled window are considered as just a “request” to the client.
  //  The client has permission to accept or ignore the request so don’t expect
  //  the request is always fulfilled. Also, window state and bounds may be
  //  altered by the client-side without any prior request from the ash-side.
  // 2. Asynchronous changes
  //  All window state/bounds changes (excluding “direct methods”) are not
  //  immediately applied but applied asynchronously when the client accepts the
  //  change request. If you want to perform something sequentially after
  //  changes, use `aura::WindowObserver::OnWindowBoundsChanged` or
  //  `WindowStateObserver::OnPostWindowStateTypeChange`.
  // 3. Direct methods
  //  `SetBoundsDirect*` directly changes the window bounds without informing
  //  the client, bypassing the client-controlled model. These methods can be
  //  useful for implementing ash-decorated window animations that the client is
  //  not interested in. However because the client is unaware of the current
  //  bounds, it may overwrite the current bounds with its preferred bounds at
  //  any time.
  bool is_client_controlled() const { return is_client_controlled_; }

  class TestApi {
   public:
    static State* GetStateImpl(WindowState* window_state) {
      return window_state->current_state_.get();
    }
  };

 private:
  friend class BaseState;
  friend class ClientControlledState;
  friend class DefaultState;
  friend class LockWindowState;
  friend class ScopedBoundsChangeAnimation;
  friend class TabletModeWindowState;
  friend class WorkspaceWindowResizerTest;
  FRIEND_TEST_ALL_PREFIXES(WindowAnimationsTest, CrossFadeToBounds);
  FRIEND_TEST_ALL_PREFIXES(WindowAnimationsTest, CrossFadeHistograms);
  FRIEND_TEST_ALL_PREFIXES(WindowAnimationsTest,
                           CrossFadeToBoundsFromTransform);

  // A class can temporarily change the window bounds change animation type.
  class ScopedBoundsChangeAnimation : public aura::WindowObserver {
   public:
    ScopedBoundsChangeAnimation(aura::Window* window,
                                BoundsChangeAnimationType animation_type);

    ScopedBoundsChangeAnimation(const ScopedBoundsChangeAnimation&) = delete;
    ScopedBoundsChangeAnimation& operator=(const ScopedBoundsChangeAnimation&) =
        delete;

    ~ScopedBoundsChangeAnimation() override;

    // aura::WindowObserver:
    void OnWindowDestroying(aura::Window* window) override;

   private:
    raw_ptr<aura::Window> window_;
    BoundsChangeAnimationType previous_bounds_animation_type_;
  };

  explicit WindowState(aura::Window* window);

  WindowStateDelegate* delegate() { return delegate_.get(); }
  BoundsChangeAnimationType bounds_animation_type() {
    return bounds_animation_type_;
  }

  // Returns the window's current z-ordering state.
  ui::ZOrderLevel GetZOrdering() const;

  // Returns the window's current show state.
  ui::mojom::WindowShowState GetShowState() const;

  // Sets the window's bounds in screen coordinates.
  void SetBoundsInScreen(const gfx::Rect& bounds_in_screen);

  // Adjusts the |bounds| so that they are flush with the edge of the
  // workspace in clamshell mode if the window represented by |window_state|
  // is side snapped. It is called for workspace events.
  void AdjustSnappedBoundsForDisplayWorkspaceChange(gfx::Rect* bounds);

  // Updates the window properties(show state, pin type) according to the
  // current window state type.
  // Note that this does not update the window bounds.
  void UpdateWindowPropertiesFromStateType();

  void NotifyPreStateTypeChange(
      chromeos::WindowStateType old_window_state_type);
  void NotifyPostStateTypeChange(
      chromeos::WindowStateType old_window_state_type);

  // Sets `bounds_in_parent` as is and ensure the layer is aligned with pixel
  // boundary.
  void SetBoundsDirect(const gfx::Rect& bounds_in_parent);

  // Sets the window's `bounds_in_parent` with constraint where the size of the
  // new bounds will not exceeds the size of the work area.
  void SetBoundsConstrained(const gfx::Rect& bounds_in_parent);

  // Sets the window's `bounds_in_parent` and transitions to the new bounds with
  // a scale animation, with duration specified by `duration`.
  void SetBoundsDirectAnimated(
      const gfx::Rect& bounds_in_parent,
      base::TimeDelta duration = kBoundsChangeSlideDuration,
      gfx::Tween::Type animation_type = gfx::Tween::LINEAR);

  // Sets the window's `bounds_in_parent` and transition to the new bounds with
  // a cross fade animation. If `float_state` has a value, sets a custom
  // float/unfloat cross fade animation.
  void SetBoundsDirectCrossFade(const gfx::Rect& bounds_in_parent,
                                std::optional<bool> float_state = std::nullopt);

  // Called before the state change and update PIP related state, such as next
  // window animation type, upon state change.
  void OnPrePipStateChange(chromeos::WindowStateType old_window_state_type);

  // Called after the state change and update PIP related state, such as next
  // window animation type, upon state change.
  void OnPostPipStateChange(chromeos::WindowStateType old_window_state_type);

  // Collects PIP enter and exit metrics:
  void CollectPipEnterExitMetrics(bool enter);

  // Records the time since partial split was started. Does nothing if the
  // window was not partial.
  void MaybeRecordPartialDuration();

  // Called after the window state changes to update the window state restore
  // history stack.
  void UpdateWindowStateRestoreHistoryStack(
      chromeos::WindowStateType previous_state_type);

  // Used in tablet mode to get the window state type depends on whether the
  // window is maximizable. If not, the window will be put in
  // `WindowStateType::kNormal` state and be centered to the work area of the
  // current display.
  chromeos::WindowStateType GetWindowTypeOnMaximizable() const;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  bool CanUnresizableSnapOnDisplay(display::Display display) const;

  void RecordWindowSnapActionSource(WindowSnapActionSource snap_action_source);

  // Gets called by the `drag_to_maximize_mis_trigger_timer_` to check the drag
  // to maximize behavior's validity and record the number of mis-triggers.
  void CheckAndRecordDragMaximizedBehavior();

  // Read out the window cycle snap action through ChromeVox. It can be snap a
  // window to the left, right or unsnapped window. `message_id` provides the
  // text will be read out.
  void ReadOutWindowCycleSnapAction(int message_id);

  // Counter used to track the number of mis-triggers of drag to maximize
  // behavior for `window_`.
  int num_of_drag_to_maximize_mis_triggers_ = 0;

  // Started when `window_` is dragged to maximized. Runs
  // `CheckAndRecordDragToMaximizeMisTriggers` to record number of mis-triggers.
  base::OneShotTimer drag_to_maximize_mis_trigger_timer_;

  // Will be set to true if `window_` has been dragged to maximized. Only when
  // it's true, we will record the number of mis-triggers of drag to maximize
  // behavior for `window_` while `this` is being destroyed.
  bool has_ever_been_dragged_to_maximized_ = false;

  // The owner of this window settings.
  raw_ptr<aura::Window> window_;
  std::unique_ptr<WindowStateDelegate> delegate_;

  bool bounds_changed_by_user_ = false;
  std::unique_ptr<DragDetails> drag_details_;

  bool unminimize_to_restore_bounds_ = false;
  bool ignore_keyboard_bounds_change_ = false;
  bool autohide_shelf_when_maximized_or_fullscreen_ = false;
  ui::ZOrderLevel cached_z_order_ = ui::ZOrderLevel::kNormal;
  bool allow_set_bounds_direct_ = false;
  bool is_moving_to_another_display_ = false;

  bool is_handling_float_event_ = false;

  // True while a snap event is being handled. Needed because a snap event can
  // trigger other events, during which we don't want the nested events to
  // update the snap ratio.
  bool is_handling_snap_event_ = false;

  // Set to false while a window may about to be unsnapped. Needed because when
  // a drag to unsnap starts, the state type is still considered snapped, but we
  // don't want to update the snap ratio with the target unsnapped bounds.
  bool can_update_snap_ratio_ = true;

  // Contains the window's target snap ratio if it's going to be snapped by a
  // WMEvent, and the updated window snap ratio if the snapped window's bounds
  // are changed while it remains snapped. It will be used to calculate the
  // desired snapped window bounds for a WMEvent, or adjust the window's bounds
  // when display or workarea changes, or decide what the window bounds should
  // be if restoring the window back to a snapped window state, etc.
  std::optional<float> snap_ratio_;

  // Contains the snap action source for the most recent snap event.
  std::optional<WindowSnapActionSource> snap_action_source_;

  // A property to remember the window position which was set before the
  // auto window position manager changed the window bounds, so that it can
  // get restored when only this one window gets shown.
  std::optional<gfx::Rect> pre_auto_manage_window_bounds_;

  // A property which resets when bounds is changed by user and sets when it
  // is nullptr, and window is removing from a workspace.
  std::optional<gfx::Rect> pre_added_to_workspace_window_bounds_;

  // A property to remember the persistent window info used in multi-displays
  // scenario to attempt to restore windows to their original bounds when
  // displays are restored to their previous states.
  std::unique_ptr<PersistentWindowInfo>
      persistent_window_info_of_display_removal_;

  // A property to remember the persistent window info when screen rotation
  // happens. It will be used to restore windows' bounds when rotating back to
  // the previous screen orientation. Note, `kLandscapePrimary` and
  // `kLandscapeSecondary` will be treated as the same screen orientation, since
  // the window's bounds should be the same in each landscape orientation. Same
  // for portrait screen orientation.
  std::unique_ptr<PersistentWindowInfo>
      persistent_window_info_of_screen_rotation_;

  base::ObserverList<WindowStateObserver>::Unchecked observer_list_;

  // True to ignore a property change event to avoid reentrance in
  // UpdateWindowStateType()
  bool ignore_property_change_ = false;

  std::unique_ptr<State> current_state_;

  // The animation type for the bounds change.
  BoundsChangeAnimationType bounds_animation_type_ =
      BoundsChangeAnimationType::kAnimate;

  // When the current (or last) PIP session started.
  base::TimeTicks pip_start_time_;

  // When the window was partial split. Not null during partial split.
  base::TimeTicks partial_start_time_;

  // Maintains the window state restore history that the current window state
  // can restore back to, with relevant restore states.
  // See `kWindowStateRestoreHistoryLayerMap` in the cc file for what window
  // state types can be put in the restore history stack.
  std::vector<chromeos::WindowStateType> window_state_restore_history_;

  // True if `current_state_` is `ClientControlledState`.
  bool is_client_controlled_{false};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_STATE_H_
