// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_KEYBOARD_UI_CONTROLLER_H_
#define ASH_KEYBOARD_UI_KEYBOARD_UI_CONTROLLER_H_

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "ash/keyboard/ui/container_behavior.h"
#include "ash/keyboard/ui/display_util.h"
#include "ash/keyboard/ui/keyboard_event_handler.h"
#include "ash/keyboard/ui/keyboard_export.h"
#include "ash/keyboard/ui/keyboard_layout_delegate.h"
#include "ash/keyboard/ui/keyboard_ui_model.h"
#include "ash/keyboard/ui/keyboard_ukm_recorder.h"
#include "ash/keyboard/ui/notification_manager.h"
#include "ash/keyboard/ui/queued_container_type.h"
#include "ash/keyboard/ui/queued_display_change.h"
#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "ash/public/cpp/keyboard/keyboard_types.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_keyboard_controller.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace aura {
class Window;
}
namespace ash {
class KeyboardControllerObserver;
}
namespace ui {
class TextInputClient;
}  // namespace ui

namespace keyboard {

class CallbackAnimationObserver;
class KeyboardUI;
class KeyboardUIFactory;

// Provides control of the virtual keyboard, including enabling/disabling the
// keyboard and controlling its visibility.
// TODO(shend): Move non-UI related logic to KeyboardControllerImpl.
class KEYBOARD_EXPORT KeyboardUIController
    : public ui::InputMethodObserver,
      public aura::WindowObserver,
      public ContainerBehavior::Delegate {
 public:
  KeyboardUIController();
  ~KeyboardUIController() override;

  // Initialize the virtual keyboard controller with two delegates:
  // - ui_factory: Responsible for keyboard window loading.
  // - layout_delegate: Responsible for moving keyboard window across displays.
  void Initialize(std::unique_ptr<KeyboardUIFactory> ui_factory,
                  KeyboardLayoutDelegate* layout_delegate);

  // Resets all the flags and disables the virtual keyboard.
  void Shutdown();

  // Retrieves the active keyboard controller. Guaranteed to not be null while
  // there is an ash::Shell.
  // TODO(stevenjb/shuchen/shend): Remove all access from src/chrome.
  // https://crbug.com/843332.
  // TODO(shend): Make this not a singleton.
  static KeyboardUIController* Get();

  // Returns true if there is a valid KeyboardUIController instance (e.g. while
  // there is an ash::Shell).
  static bool HasInstance();

  // Returns the keyboard window, or null if the keyboard window has not been
  // created yet.
  aura::Window* GetKeyboardWindow() const;

  // Returns the root window that this keyboard controller is attached to, or
  // null if the keyboard has not been attached to any root window.
  aura::Window* GetRootWindow() const;

  // Move the keyboard window to a different parent container. |parent| must not
  // be null.
  void MoveToParentContainer(aura::Window* parent);

  // Sets the bounds of the keyboard window, relative to the root window.
  void SetKeyboardWindowBounds(const gfx::Rect& new_bounds_in_root);

  // Reloads the content of the keyboard. No-op if the keyboard content is not
  // loaded yet.
  void Reload();

  // Rebuilds the keyboard by disabling and enabling it again.
  // TODO(https://crbug.com/845780): Can this be replaced with |Reload|?
  void RebuildKeyboardIfEnabled();

  // Management of the observer list.
  void AddObserver(ash::KeyboardControllerObserver* observer);
  bool HasObserver(ash::KeyboardControllerObserver* observer) const;
  void RemoveObserver(ash::KeyboardControllerObserver* observer);

  // Updates |keyboard_config_| with |config|. Returns |false| if there is no
  // change, otherwise returns true and notifies observers if this is enabled.
  bool UpdateKeyboardConfig(const KeyboardConfig& config);
  const KeyboardConfig& keyboard_config() { return keyboard_config_; }

  // Sets and clears |keyboard_enable_flags_| entries.
  void SetEnableFlag(KeyboardEnableFlag flag);
  void ClearEnableFlag(KeyboardEnableFlag flag);
  bool IsEnableFlagSet(KeyboardEnableFlag flag) const;
  const std::set<KeyboardEnableFlag>& keyboard_enable_flags() const {
    return keyboard_enable_flags_;
  }

  // Returns true if keyboard overscroll is enabled.
  bool IsKeyboardOverscrollEnabled() const;

  // Hide the keyboard because the user has chosen to specifically hide the
  // keyboard, such as pressing the dismiss button.
  // TODO(https://crbug.com/845780): Rename this to
  // HideKeyboardExplicitlyByUser.
  // TODO(https://crbug.com/845780): Audit and switch callers to
  // HideKeyboardImplicitlyByUser where appropriate.
  void HideKeyboardByUser();

  // Hide the keyboard as a secondary effect of a user action, such as tapping
  // the shelf. The keyboard should not hide if it's locked.
  void HideKeyboardImplicitlyByUser();

  // Hide the keyboard due to some internally generated change to change the
  // state of the keyboard. For example, moving from the docked keyboard to the
  // floating keyboard.
  void HideKeyboardTemporarilyForTransition();

  // Hide the keyboard as an effect of a system action, such as opening the
  // settings page from the keyboard. There should be no reason the keyboard
  // should remain open.
  void HideKeyboardExplicitlyBySystem();

  // Hide the keyboard as a secondary effect of a system action, such as losing
  // focus of a text element. If focus is returned to any text element, it is
  // desirable to re-show the keyboard in this case.
  void HideKeyboardImplicitlyBySystem();

  // Force the keyboard to show up if not showing and lock the keyboard if
  // |lock| is true.
  void ShowKeyboard(bool lock);

  // Force the keyboard to show up in the specific display if not showing and
  // lock the keyboard
  void ShowKeyboardInDisplay(const display::Display& display);

  // Returns the bounds in root window for the visible portion of the keyboard.
  // An empty rectangle will get returned when the keyboard is hidden.
  const gfx::Rect& visual_bounds_in_root() const {
    return visual_bounds_in_root_;
  }

  // Returns the bounds in screen for the visible portion of the keyboard. An
  // empty rectangle will get returned when the keyboard is hidden.
  gfx::Rect GetVisualBoundsInScreen() const;

  // Returns the current bounds that affect the workspace layout. If the
  // keyboard is not shown or if the keyboard mode should not affect the usable
  // region of the screen, an empty rectangle will be returned. Bounds are
  // relative to the screen.
  gfx::Rect GetWorkspaceOccludedBoundsInScreen() const;

  // Returns the current bounds that affect the window layout of the various
  // lock screens. Bounds are relative to the root window.
  gfx::Rect GetKeyboardLockScreenOffsetBounds() const;

  // Set the area on the keyboard window that occlude whatever is behind it.
  void SetOccludedBounds(const gfx::Rect& bounds_in_window);

  // Set the areas on the keyboard window where events should be handled.
  // Does not do anything if there is no keyboard window. Bounds are relative to
  // the keyboard window.
  void SetHitTestBounds(const std::vector<gfx::Rect>& bounds_in_window);

  // Sets the area of the keyboard window that should not move off screen. Any
  // area outside of this can be moved off the user's screen. Note the bounds
  // here are relative to the window's origin.
  bool SetAreaToRemainOnScreen(const gfx::Rect& bounds_in_window);

  ContainerType GetActiveContainerType() const {
    return container_behavior_->GetType();
  }

  // Adjusts |requested_bounds| according to the current container behavior.
  // (e.g. prevent the keyboard from moving off screen).
  gfx::Rect AdjustSetBoundsRequest(
      const gfx::Rect& display_bounds,
      const gfx::Rect& requested_bounds_in_screen) const;

  // Returns true if overscroll is currently allowed by the active keyboard
  // container behavior.
  bool IsOverscrollAllowed() const;

  // Whether the keyboard has been enabled, i.e. EnableKeyboard() has been
  // called.
  bool IsEnabled() const { return ui_ != nullptr; }

  // Handle mouse and touch events on the keyboard. The effects of this method
  // will not stop propagation to the keyboard extension.
  bool HandlePointerEvent(const ui::LocatedEvent& event);

  // Sets the active container type. If the keyboard is currently shown, this
  // will trigger a hide animation and a subsequent show animation. Otherwise
  // the ContainerBehavior change is synchronous.
  void SetContainerType(ContainerType type,
                        const base::Optional<gfx::Rect>& target_bounds_in_root,
                        base::OnceCallback<void(bool)> callback);

  // Sets floating keyboard draggable rect.
  void SetDraggableArea(const gfx::Rect& rect);

  bool IsKeyboardVisible();

  void KeyboardContentsLoaded();

  aura::Window* parent_container() { return parent_container_; }

  ui::InputMethodKeyboardController* input_method_keyboard_controller() {
    return input_method_keyboard_controller_.get();
  }

  bool keyboard_locked() const { return keyboard_locked_; }
  void set_keyboard_locked(bool lock) { keyboard_locked_ = lock; }

  void set_container_behavior_for_test(
      std::unique_ptr<ContainerBehavior> container_behavior) {
    container_behavior_ = std::move(container_behavior);
  }
  KeyboardUIState GetStateForTest() const { return model_.state(); }
  ui::InputMethod* GetInputMethodForTest();
  void EnsureCaretInWorkAreaForTest(const gfx::Rect& occluded_bounds_in_screen);

 private:
  // For access to Observer methods for simulation.
  friend class KeyboardUIControllerTest;

  // Different ways to hide the keyboard.
  enum HideReason {
    // System initiated due to an active event, where the user does not want
    // to maintain any association with the previous text entry session.
    HIDE_REASON_SYSTEM_EXPLICIT,

    // System initiated due to a passive event, such as clicking on a non-text
    // control in a web page. Implicit hide events can be treated as passive
    // and can possibly be a transient loss of focus. This will generally cause
    // the keyboard to stay open for a brief moment and then hide, and possibly
    // come back if focus is regained within a short amount of time (transient
    // blur).
    HIDE_REASON_SYSTEM_IMPLICIT,

    // Keyboard is hidden temporarily for transitional reasons. Examples include
    // moving the keyboard to a different display (which closes it and re-opens
    // it on the new screen) or changing the container type (e.g. full-width to
    // floating)
    HIDE_REASON_SYSTEM_TEMPORARY,

    // User explicitly hiding the keyboard via the close button. Also hides
    // locked keyboards.
    HIDE_REASON_USER_EXPLICIT,

    // Keyboard is hidden as an indirect consequence of some user action.
    // Examples include opening the window overview mode, or tapping on the
    // shelf status area. Does not hide locked keyboards.
    HIDE_REASON_USER_IMPLICIT,
  };

  // ContainerBehavior::Delegate overrides
  bool IsKeyboardLocked() const override;
  gfx::Rect GetBoundsInScreen() const override;
  void MoveKeyboardWindow(const gfx::Rect& new_bounds) override;
  void MoveKeyboardWindowToDisplay(const display::Display& display,
                                   const gfx::Rect& new_bounds) override;

  // aura::WindowObserver overrides
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // InputMethodObserver overrides
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnFocus() override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override;
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnShowVirtualKeyboardIfEnabled() override;

  // Enables the virtual keyboard.
  // Immediately starts pre-loading the keyboard window in the background.
  // Does nothing if the keyboard is already enabled.
  void EnableKeyboard();

  // Disables the virtual keyboard. Resets the keyboard to its initial disabled
  // state and destroys the keyboard window.
  // Does nothing if the keyboard is already disabled.
  void DisableKeyboard();

  // Returns true if the keyboard should be enabled, i.e. the current result
  // of Set/ClearEnableFlag should cause the keyboard to be enabled.
  bool IsKeyboardEnableRequested() const;

  // Enables or disables the keyboard based on |IsKeyboardEnableRequested|,
  // as requested by |flag|.
  void UpdateKeyboardAsRequestedBy(KeyboardEnableFlag flag);

  // Attach the keyboard window as a child of the given parent window.
  // Can only be called when the keyboard is not activated. |parent| must not
  // have any children.
  void ActivateKeyboardInContainer(aura::Window* parent);

  // Detach the keyboard window from its parent container window.
  // Can only be called when the keyboard is activated. Explicitly hides the
  // keyboard if it is currently visible.
  void DeactivateKeyboard();

  // Loads the keyboard window in the background, but does not display
  // the keyboard.
  void LoadKeyboardWindowInBackground();

  // Show virtual keyboard immediately with animation.
  void ShowKeyboardInternal(aura::Window* target_container);
  void PopulateKeyboardContent(aura::Window* target_container);

  // Returns true if keyboard is scheduled to hide.
  bool WillHideKeyboard() const;

  // Immediately starts hiding animation of virtual keyboard and notifies
  // observers bounds change. This method forcibly sets keyboard_locked_
  // false while closing the keyboard.
  void HideKeyboard(HideReason reason);

  // Called when the hide animation finished.
  void HideAnimationFinished();
  // Called when the show animation finished.
  void ShowAnimationFinished();

  // Notifies keyboard config change to the observers.
  // Only called from |UpdateKeyboardConfig| in keyboard_util.
  void NotifyKeyboardConfigChanged();

  // Notifies observers that the visual or occluded bounds of the keyboard
  // window are changing.
  void NotifyKeyboardBoundsChanging(const gfx::Rect& new_bounds_in_root);

  // Called when the keyboard window has loaded. Shows the keyboard if
  // |show_on_keyboard_window_load_| is true.
  void NotifyKeyboardWindowLoaded();

  // Validates the state transition. Called from ChangeState.
  void CheckStateTransition(KeyboardUIState prev, KeyboardUIState next);

  // Changes the current state and validates the transition.
  void ChangeState(KeyboardUIState state);

  // Reports error histogram in case lingering in an intermediate state.
  void ReportLingeringState();

  // Shows the keyboard if the last time the keyboard was hidden was a small
  // time ago.
  void ShowKeyboardIfWithinTransientBlurThreshold();

  void SetContainerBehaviorInternal(ContainerType type);

  // Records that keyboard was shown on the currently focused UKM source.
  void RecordUkmKeyboardShown();

  // Gets the currently focused text input client.
  ui::TextInputClient* GetTextInputClient();

  // Ensures that the current IME is observed if it is changed.
  void UpdateInputMethodObserver();

  // Ensures caret in current work area (not occluded by virtual keyboard
  // window).
  void EnsureCaretInWorkArea(const gfx::Rect& occluded_bounds_in_screen);

  // Marks that the keyboard load has started. This is used to measure the time
  // it takes to fully load the keyboard. This should be called before
  // MarkKeyboardLoadFinished.
  void MarkKeyboardLoadStarted();

  // Marks that the keyboard load has ended. This finishes measuring that the
  // keyboard is loaded.
  void MarkKeyboardLoadFinished();

  // Called when the enable flags change. Notifies observers of the change.
  void EnableFlagsChanged();

  std::unique_ptr<KeyboardUIFactory> ui_factory_;
  std::unique_ptr<KeyboardUI> ui_;
  std::unique_ptr<ui::InputMethodKeyboardController>
      input_method_keyboard_controller_;
  KeyboardLayoutDelegate* layout_delegate_ = nullptr;
  ScopedObserver<ui::InputMethod, ui::InputMethodObserver> ime_observer_{this};

  // Container window that the keyboard window is a child of.
  aura::Window* parent_container_ = nullptr;

  // CallbackAnimationObserver should be destroyed before |ui_| because it uses
  // |ui_|'s animator.
  std::unique_ptr<CallbackAnimationObserver> animation_observer_;

  // Current active visual behavior for the keyboard container.
  std::unique_ptr<ContainerBehavior> container_behavior_;

  std::unique_ptr<QueuedContainerType> queued_container_type_;
  std::unique_ptr<QueuedDisplayChange> queued_display_change_;

  // If true, show the keyboard window when it loads.
  bool show_on_keyboard_window_load_ = false;

  // If true, the keyboard is always visible even if no window has input focus.
  // TODO(https://crbug.com/964191): Move this to the UI model.
  bool keyboard_locked_ = false;
  KeyboardEventHandler event_handler_;

  base::ObserverList<ash::KeyboardControllerObserver>::Unchecked observer_list_;

  KeyboardUIModel model_;

  // The bounds for the visible portion of the keyboard, relative to the root
  // window. If the keyboard window is visible, this should be the same size as
  // the keyboard window. If not, this should be empty.
  gfx::Rect visual_bounds_in_root_;

  // Keyboard configuration associated with the controller.
  KeyboardConfig keyboard_config_;

  // Set of active enabled request flags. Used to determine whether the keyboard
  // should be enabled.
  std::set<KeyboardEnableFlag> keyboard_enable_flags_;

  NotificationManager notification_manager_;

  base::Time time_of_last_blur_ = base::Time::UnixEpoch();

  DisplayUtil display_util_;

  bool keyboard_load_time_logged_ = false;
  base::Time keyboard_load_time_start_;

  base::WeakPtrFactory<KeyboardUIController>
      weak_factory_report_lingering_state_{this};
  base::WeakPtrFactory<KeyboardUIController> weak_factory_will_hide_{this};

  DISALLOW_COPY_AND_ASSIGN(KeyboardUIController);
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_KEYBOARD_UI_CONTROLLER_H_
