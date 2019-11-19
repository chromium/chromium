// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_STATE_H_
#define ASH_WM_WINDOW_STATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/display/persistent_window_info.h"
#include "ash/wm/drag_details.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer_owner.h"
#include "ui/display/display.h"
#include "ui/gfx/animation/tween.h"

namespace gfx {
class Rect;
}

namespace ash {
class ClientControlledState;
class LockWindowState;
class TabletModeWindowState;
enum class WindowPinType;
class WindowState;
class WindowStateDelegate;
class WindowStateObserver;
enum class WindowStateType;
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
  // The default duration for an animation between two sets of bounds.
  static constexpr base::TimeDelta kBoundsChangeSlideDuration =
      base::TimeDelta::FromMilliseconds(120);

  // A subclass of State class represents one of the window's states
  // that corresponds to WindowStateType in Ash environment, e.g.
  // maximized, minimized or side snapped, as subclass.
  // Each subclass defines its own behavior and transition for each WMEvent.
  class State {
   public:
    State() {}
    virtual ~State() {}

    // Update WindowState based on |event|.
    virtual void OnWMEvent(WindowState* window_state, const WMEvent* event) = 0;

    virtual WindowStateType GetType() const = 0;

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

   private:
    DISALLOW_COPY_AND_ASSIGN(State);
  };

  // Returns the WindowState for |window|. Creates WindowState if it doesn't
  // exist. The returned value is owned by |window| (you should not delete it).
  static WindowState* Get(aura::Window* window);
  static const WindowState* Get(const aura::Window* window);

  // Returns the WindowState for the active window, null if there is no active
  // window.
  static WindowState* ForActiveWindow();

  // Call WindowState::Get() to instantiate this class.
  ~WindowState() override;

  aura::Window* window() { return window_; }
  const aura::Window* window() const { return window_; }

  bool HasDelegate() const;
  void SetDelegate(std::unique_ptr<WindowStateDelegate> delegate);

  // Returns the window's current ash state type.
  // Refer to WindowStateType definition in wm_types.h as for why Ash
  // has its own state type.
  WindowStateType GetStateType() const;

  // Predicates to check window state.
  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsFullscreen() const;
  bool IsSnapped() const;
  bool IsPinned() const;
  bool IsTrustedPinned() const;
  bool IsPip() const;

  // True if the window's state type is WindowStateType::kMaximized,
  // WindowStateType::kFullscreen or WindowStateType::kPinned.
  bool IsMaximizedOrFullscreenOrPinned() const;

  // True if the window's state type is WindowStateType::kNormal or
  // WindowStateType::kDefault.
  bool IsNormalStateType() const;

  bool IsNormalOrSnapped() const;

  bool IsActive() const;

  // Returns true if the window's location can be controlled by the user.
  bool IsUserPositionable() const;

  // Checks if the window can change its state accordingly.
  bool CanMaximize() const;
  bool CanMinimize() const;
  bool CanResize() const;
  bool CanSnap() const;
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

  // Set the window state to normal.
  // TODO(oshima): Change to use RESTORE event.
  void Restore();

  // Caches, then disables z-ordering state and then stacks |window_| below
  // |window_on_top| if |window_| currently has a special z-order.
  void DisableZOrdering(aura::Window* window_on_top);

  // Restores the z-ordering state that a window might have cached.
  void RestoreZOrdering();

  // Invoked when a WMevent occurs, which drives the internal
  // state machine.
  void OnWMEvent(const WMEvent* event);

  // TODO(oshima): Try hiding these methods and making them accessible only to
  // state impl. State changes should happen through events (as much
  // as possible).

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

  // Replace the State object of a window with a state handler which can
  // implement a new window manager type. The passed object will be owned
  // by this object and the returned object will be owned by the caller.
  std::unique_ptr<State> SetStateObject(std::unique_ptr<State> new_state);

  // Updates |snapped_width_ratio_| based on |event|.
  void UpdateSnappedWidthRatio(const WMEvent* event);
  base::Optional<float> snapped_width_ratio() const {
    return snapped_width_ratio_;
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

  // Gets/sets whether the shelf should be hidden when this window is
  // fullscreen.
  bool GetHideShelfWhenFullscreen() const;
  void SetHideShelfWhenFullscreen(bool value);

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
  const base::Optional<gfx::Rect> pre_auto_manage_window_bounds() {
    return pre_auto_manage_window_bounds_;
  }
  void SetPreAutoManageWindowBounds(const gfx::Rect& bounds);

  // Gets/Sets the property that is used on window added to workspace event.
  const base::Optional<gfx::Rect> pre_added_to_workspace_window_bounds() {
    return pre_added_to_workspace_window_bounds_;
  }
  void SetPreAddedToWorkspaceWindowBounds(const gfx::Rect& bounds);

  // Gets/Sets the persistent window info that is used on restoring persistent
  // window bounds in multi-displays scenario.
  const base::Optional<PersistentWindowInfo> persistent_window_info() {
    return persistent_window_info_;
  }
  void SetPersistentWindowInfo(
      const PersistentWindowInfo& persistent_window_info);
  void ResetPersistentWindowInfo();

  // Layout related properties

  void AddObserver(WindowStateObserver* observer);
  void RemoveObserver(WindowStateObserver* observer);

  // Whether the window is being dragged.
  bool is_dragged() const { return !!drag_details_; }

  // Whether or not the window's position can be managed by the
  // auto management logic.
  bool GetWindowPositionManaged() const;
  void SetWindowPositionManaged(bool managed);

  // Whether or not the window's position or size was changed by a user.
  bool bounds_changed_by_user() const { return bounds_changed_by_user_; }
  void set_bounds_changed_by_user(bool bounds_changed_by_user);

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

  // Creates and takes ownership of a pointer to DragDetails when resizing is
  // active. This should be done before a resizer gets created.
  void CreateDragDetails(const gfx::Point& point_in_parent,
                         int window_component,
                         ::wm::WindowMoveSource source);

  // Deletes and clears a pointer to DragDetails. This should be done when the
  // resizer gets destroyed.
  void DeleteDragDetails();

  // Sets the currently stored restore bounds and clears the restore bounds.
  void SetAndClearRestoreBounds();

  // Notifies that the drag operation has been started.
  void OnDragStarted(int window_component);

  // Notifies that the drag operation has been either completed or reverted.
  // |location| is the last position of the pointer device used to drag.
  void OnCompleteDrag(const gfx::Point& location);
  void OnRevertDrag(const gfx::Point& location);

  // Notifies that the window lost the activation.
  void OnActivationLost();

  // Returns a pointer to DragDetails during drag operations.
  const DragDetails* drag_details() const { return drag_details_.get(); }
  DragDetails* drag_details() { return drag_details_.get(); }

  void set_animation_smoothness_histogram_name(
      base::Optional<std::string> val) {
    animation_smoothness_histogram_name_ = val;
  }

  // Returns the Display that this WindowState is on.
  display::Display GetDisplay();

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
  friend class TabletModeWindowState;
  friend class ScopedBoundsChangeAnimation;
  FRIEND_TEST_ALL_PREFIXES(WindowAnimationsTest, CrossFadeToBounds);
  FRIEND_TEST_ALL_PREFIXES(WindowAnimationsTest,
                           CrossFadeToBoundsFromTransform);
  FRIEND_TEST_ALL_PREFIXES(WindowStateTest, PipWindowMaskRecreated);
  FRIEND_TEST_ALL_PREFIXES(WindowStateTest, PipWindowHasMaskLayer);

  // Animation type of updating window bounds. "IMMEDIATE" means update bounds
  // directly without animation. "STEP_END" means update bounds at the end of
  // the animation.
  enum class BoundsChangeAnimationType { DEFAULT, IMMEDIATE, STEP_END };

  // A class can temporarily change the window bounds change animation type.
  class ScopedBoundsChangeAnimation : public aura::WindowObserver {
   public:
    ScopedBoundsChangeAnimation(aura::Window* window,
                                BoundsChangeAnimationType animation_type);
    ~ScopedBoundsChangeAnimation() override;

    // aura::WindowObserver:
    void OnWindowDestroying(aura::Window* window) override;

   private:
    aura::Window* window_;
    BoundsChangeAnimationType previous_bounds_animation_type_;

    DISALLOW_COPY_AND_ASSIGN(ScopedBoundsChangeAnimation);
  };

  explicit WindowState(aura::Window* window);

  WindowStateDelegate* delegate() { return delegate_.get(); }
  BoundsChangeAnimationType bounds_animation_type() {
    return bounds_animation_type_;
  }

  bool HasMaximumWidthOrHeight() const;

  // Returns the window's current z-ordering state.
  ui::ZOrderLevel GetZOrdering() const;

  // Returns the window's current show state.
  ui::WindowShowState GetShowState() const;

  // Return the window's current pin type.
  ash::WindowPinType GetPinType() const;

  // Sets the window's bounds in screen coordinates.
  void SetBoundsInScreen(const gfx::Rect& bounds_in_screen);

  // Adjusts the |bounds| so that they are flush with the edge of the
  // workspace if the window represented by |window_state| is side snapped. It
  // is called for workspace events.
  void AdjustSnappedBounds(gfx::Rect* bounds);

  // Updates the window properties(show state, pin type) according to the
  // current window state type.
  // Note that this does not update the window bounds.
  void UpdateWindowPropertiesFromStateType();

  void NotifyPreStateTypeChange(WindowStateType old_window_state_type);
  void NotifyPostStateTypeChange(WindowStateType old_window_state_type);

  // Sets |bounds| as is and ensure the layer is aligned with pixel boundary.
  void SetBoundsDirect(const gfx::Rect& bounds);

  // Sets the window's |bounds| with constraint where the size of the
  // new bounds will not exceeds the size of the work area.
  void SetBoundsConstrained(const gfx::Rect& bounds);

  // Sets the wndow's |bounds| and transitions to the new bounds with
  // a scale animation, with duration specified by |duration|.
  void SetBoundsDirectAnimated(
      const gfx::Rect& bounds,
      base::TimeDelta duration = kBoundsChangeSlideDuration,
      gfx::Tween::Type animation_type = gfx::Tween::LINEAR);

  // Sets the window's |bounds| and transition to the new bounds with
  // a cross fade animation.
  void SetBoundsDirectCrossFade(const gfx::Rect& bounds);

  // Called before the state change and update PIP related state, such as next
  // window animation type, upon state change.
  void OnPrePipStateChange(WindowStateType old_window_state_type);

  // Called after the state change and update PIP related state, such as next
  // window animation type, upon state change.
  void OnPostPipStateChange(WindowStateType old_window_state_type);

  // Update the PIP bounds if necessary. This may need to happen when the
  // display work area changes, or if system ui regions like the virtual
  // keyboard position changes.
  void UpdatePipBounds();

  // Collects PIP enter and exit metrics:
  void CollectPipEnterExitMetrics(bool enter);

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  // The owner of this window settings.
  aura::Window* window_;
  std::unique_ptr<WindowStateDelegate> delegate_;

  bool bounds_changed_by_user_;
  bool can_consume_system_keys_;
  std::unique_ptr<DragDetails> drag_details_;

  // If this has a value when an animation starts, animation smoothness metrics
  // with this name will be logged for the animation.
  base::Optional<std::string> animation_smoothness_histogram_name_;

  bool unminimize_to_restore_bounds_;
  bool ignore_keyboard_bounds_change_ = false;
  bool hide_shelf_when_fullscreen_;
  bool autohide_shelf_when_maximized_or_fullscreen_;
  ui::ZOrderLevel cached_z_order_;
  bool allow_set_bounds_direct_ = false;

  // A property to save the ratio between snapped window width and display
  // workarea width. It is used to update snapped window width on
  // AdjustSnappedBounds() when handling workspace events.
  base::Optional<float> snapped_width_ratio_;

  // A property to remember the window position which was set before the
  // auto window position manager changed the window bounds, so that it can
  // get restored when only this one window gets shown.
  base::Optional<gfx::Rect> pre_auto_manage_window_bounds_;

  // A property which resets when bounds is changed by user and sets when it
  // is nullptr, and window is removing from a workspace.
  base::Optional<gfx::Rect> pre_added_to_workspace_window_bounds_;

  // A property to remember the persistent window info used in multi-displays
  // scenario to attempt to restore windows to their original bounds when
  // displays are restored to their previous states.
  base::Optional<PersistentWindowInfo> persistent_window_info_;

  base::ObserverList<WindowStateObserver>::Unchecked observer_list_;

  // True to ignore a property change event to avoid reentrance in
  // UpdateWindowStateType()
  bool ignore_property_change_;

  std::unique_ptr<State> current_state_;

  // The animation type for the bounds change.
  BoundsChangeAnimationType bounds_animation_type_ =
      BoundsChangeAnimationType::DEFAULT;

  // When the current (or last) PIP session started.
  base::TimeTicks pip_start_time_;

  DISALLOW_COPY_AND_ASSIGN(WindowState);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_STATE_H_
