// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_CONTROLLER_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace ash {

class WindowCycleEventFilter;
class WindowCycleList;

// Controls cycling through windows with the keyboard via alt-tab.
// Windows are sorted primarily by most recently used, and then by screen order.
// We activate windows as you cycle through them, so the order on the screen
// may change during the gesture, but the most recently used list isn't updated
// until the cycling ends.  Thus we maintain the state of the windows
// at the beginning of the gesture so you can cycle through in a consistent
// order.
class ASH_EXPORT WindowCycleController : public SessionObserver,
                                         public DesksController::Observer {
 public:
  using WindowList = std::vector<raw_ptr<aura::Window, VectorExperimental>>;

  enum class WindowCyclingDirection { kForward, kBackward };
  enum class KeyboardNavDirection { kUp, kDown, kLeft, kRight, kInvalid };

  // Enumeration of the sources of alt-tab mode switch.
  // Note that these values are persisted to histograms so existing values
  // should remain unchanged and new values should be added to the end.
  enum class ModeSwitchSource { kClick, kKeyboard, kMaxValue = kKeyboard };

  WindowCycleController();

  WindowCycleController(const WindowCycleController&) = delete;
  WindowCycleController& operator=(const WindowCycleController&) = delete;

  ~WindowCycleController() override;

  // Returns true if cycling through windows is enabled. This is false at
  // certain times, such as when the lock screen is visible.
  static bool CanCycle();

  // Registers alt-tab related profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns the WindowCycleList.
  const WindowCycleList* window_cycle_list() const {
    return window_cycle_list_.get();
  }

  // Cycles between windows in the given |direction|. This moves the focus ring
  // to the window in the given |direction| and also scrolls the list. If
  // same_app_only is provided whether or not to cycle exclusively between
  // windows of the same app from now on will be updated.
  void HandleCycleWindow(WindowCyclingDirection direction,
                         bool same_app_only = false);

  // Navigates between cycle windows and tab slider if the move is valid.
  // This moves the focus ring to the active button or the last focused window
  // and announces these changes via ChromeVox.
  void HandleKeyboardNavigation(KeyboardNavDirection direction);

  // Drags the cycle view's mirror container |delta_x|.
  void Drag(float delta_x);

  // Starts a fling for the cycle view's mirror container base on |velocity_x|.
  void StartFling(float velocity_x);

  // Returns true if we are in the middle of a window cycling gesture.
  bool IsCycling() const { return window_cycle_list_.get() != NULL; }

  // Call to start cycling windows. This function adds a pre-target handler to
  // listen to the alt key release.
  void StartCycling(bool same_app_only);

  // Both of these functions stop the current window cycle and removes the event
  // filter. The former indicates success (i.e. the new window should be
  // activated) and the latter indicates that the interaction was cancelled (and
  // the originally active window should remain active).
  void CompleteCycling();
  void CancelCycling();

  // If the window cycle list is open, re-construct it. Do nothing if not
  // cycling.
  void MaybeResetCycleList();

  // Moves the focus ring to |window|. Does not scroll the list. Do nothing if
  // not cycling.
  void SetFocusedWindow(aura::Window* window);

  // Checks whether |event| occurs within the cycle view.
  bool IsEventInCycleView(const ui::LocatedEvent* event) const;

  // Gets the window for the preview item located at |event|. Returns nullptr if
  // |event| is not on the cycle view or a preview item, or |window_cycle_list_|
  // does not exist.
  aura::Window* GetWindowAtPoint(const ui::LocatedEvent* event);

  // Returns whether or not the event is located in tab slider container.
  bool IsEventInTabSliderContainer(const ui::LocatedEvent* event) const;

  // Returns whether or not the window cycle view is visible.
  bool IsWindowListVisible() const;

  // Checks if switching between alt-tab mode via the tab slider is allowed.
  // Returns true if Bento flag is enabled and users have multiple desks.
  bool IsInteractiveAltTabModeAllowed() const;

  // Checks if alt-tab should be per active desk. If
  // `IsInteractiveAltTabModeAllowed()`, alt-tab mode depends on users'
  // |prefs::kAltTabPerDesk| selection. Otherwise, it'll default to all desk
  // unless LimitAltTabToActiveDesk flag is explicitly enabled.
  bool IsAltTabPerActiveDesk() const;

  // Returns true while switching the alt-tab mode and Bento flag is enabled.
  // This helps `Scroll()` and `Step()` distinguish between pressing tabs and
  // switching mode, so they refresh `current_index_` and the focused window
  // correctly.
  bool IsSwitchingMode() const;

  // Returns if the tab slider is currently focused instead of the window cycle
  // during keyboard navigation.
  bool IsTabSliderFocused() const;

  // Saves `per_desk` in the user prefs and announces changes of alt-tab mode
  // and the window selection via ChromeVox. This function is called when the
  // user switches the alt-tab mode via keyboard navigation or button clicking.
  void OnModeChanged(bool per_desk, ModeSwitchSource source);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk, bool from_undo) override;
  void OnDeskRemoved(const Desk* desk) override;

 private:
  friend class WindowCycleList;

  // Gets a list of windows from the currently open windows, removing windows
  // with transient roots already in the list. The returned list of windows
  // is used to populate the window cycle list.
  WindowList CreateWindowList();

  // Builds the window list for window cycling, `desks_mru_type` determines
  // whether to include or exclude windows from the inactive desks. The list is
  // built based on `BuildWindowForCycleWithPipList()` and revised so that
  // windows in a snap group are put together with primary window comes before
  // secondary snapped window.
  WindowList BuildWindowListForWindowCycling(DesksMruType desks_mru_type);

  // Populates |active_desk_container_id_before_cycle_| and
  // |active_window_before_window_cycle_| when the window cycle list is
  // initialized.
  void SaveCurrentActiveDeskAndWindow(const WindowList& window_list);

  // Cycles to the next or previous window based on |direction| or to the
  // default position if |starting_alt_tab_or_switching_mode| is true.
  // This updates the focus ring to the window to the right if |direction|
  // is forward or left if backward. If |starting_alt_tab_or_switching_mode| is
  // true and |direction| is forward, the focus ring moves to the first
  // non-active window in MRU list: the second window by default or the first
  // window if it is not active.
  void Step(WindowCyclingDirection direction,
            bool starting_alt_tab_or_switching_mode);

  void StopCycling();

  void InitFromUserPrefs();

  // Triggers alt-tab UI updates when the alt-tab mode is updated in the active
  // user prefs.
  void OnAltTabModePrefChanged();

  // Returns true if the direction is valid regarding the component that the
  // focus is currently on. For example, moving the focus on the top most
  // component, the tab slider button, further up is invalid.
  bool IsValidKeyboardNavigation(KeyboardNavDirection direction) const;

  std::unique_ptr<WindowCycleList> window_cycle_list_;

  // Tracks the ID of the active desk container before window cycling starts. It
  // is used to determine whether a desk switch occurred when cycling ends.
  int active_desk_container_id_before_cycle_ = kShellWindowId_Invalid;

  // Tracks what Window was active when starting to cycle and used to determine
  // if the active Window changed in when ending cycling.
  raw_ptr<aura::Window, DanglingUntriaged> active_window_before_window_cycle_ =
      nullptr;

  // Non-null while actively cycling.
  std::unique_ptr<WindowCycleEventFilter> event_filter_;

  // Tracks whether alt-tab mode is currently switching or not.
  bool is_switching_mode_ = false;

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  raw_ptr<PrefService> active_user_pref_service_ = nullptr;

  // The pref change registrar to observe changes in prefs value.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ScopedObservation<DesksController, DesksController::Observer>
      desks_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_CONTROLLER_H_
