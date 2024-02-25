// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_LIST_H_
#define ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_LIST_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/event.h"

namespace aura {
class ScopedWindowTargeter;
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {

class WindowCycleView;

using WindowCyclingDirection = WindowCycleController::WindowCyclingDirection;

// Tracks a set of Windows that can be stepped through. This class is used by
// the WindowCycleController.
class ASH_EXPORT WindowCycleList : public aura::WindowObserver,
                                   public display::DisplayObserver {
 public:
  using WindowList = std::vector<raw_ptr<aura::Window, VectorExperimental>>;

  WindowCycleList(const WindowList& windows, bool same_app_only);
  WindowCycleList(const WindowCycleList&) = delete;
  WindowCycleList& operator=(const WindowCycleList&) = delete;
  ~WindowCycleList() override;

  void set_user_did_accept(bool user_did_accept) {
    user_did_accept_ = user_did_accept;
  }

  bool same_app_only() const { return same_app_only_; }

  const WindowCycleView* cycle_view() const { return cycle_view_; }

  // Returns the |target_window_| from |cycle_view_|.
  aura::Window* GetTargetWindow();

  // Removes the existing windows and replaces them with |windows|. If
  // |windows| is empty, cancels cycling.
  void ReplaceWindows(const WindowList& windows);

  // Cycles to the next or previous window based on |direction| or to the
  // default position if |starting_alt_tab_or_switching_mode| is true.
  // This moves the focus ring and also scrolls the list.
  // If |starting_alt_tab_or_switching_mode| is true and |direction| is
  // forward, the focus ring moves to the first non-active window in MRU list:
  // the second window by default or the first window if it is not active.
  void Step(WindowCyclingDirection direction,
            bool starting_alt_tab_or_switching_mode);

  // Should be called when a user drags their finger on the touch screen.
  // Translates the mirror container by |delta_x|.
  void Drag(float delta_x);

  // Beings a fling with initial velocity of |velocity_x|.
  void StartFling(float velocity_x);

  // Moves the focus ring to the respective preview for |window|. Does not
  // scroll the window cycle list.
  void SetFocusedWindow(aura::Window* window);

  // Moves the focus to the tab slider or the window cycle list based on
  // |focus| value during keyboard navigation.
  void SetFocusTabSlider(bool focus);

  // Returns true if during keyboard navigation, alt-tab focuses the tab slider
  // instead of cycle window.
  bool IsTabSliderFocused() const;

  // Checks whether |event| occurs within the cycle view. Returns false if
  // |cycle_view_| does not exist.
  bool IsEventInCycleView(const ui::LocatedEvent* event) const;

  // Returns the window for the preview item located at |event|. Returns nullptr
  // if |event| not in cycle view or if |cycle_view_| does not exist.
  aura::Window* GetWindowAtPoint(const ui::LocatedEvent* event);

  // Returns whether or not the event is located in tab slider container.
  bool IsEventInTabSliderContainer(const ui::LocatedEvent* event) const;

  // Returns true if the window list overlay should be shown.
  bool ShouldShowUi() const;

  // Updates the tab slider mode UI when alt-tab mode in user prefs changes.
  void OnModePrefsChanged();

  static void SetDisableInitialDelayForTesting(bool disabled);

  const WindowList& windows_for_testing() const { return windows_; }

 private:
  friend class ModeSelectionWindowCycleControllerTest;
  friend class MultiUserWindowCycleControllerTest;
  friend class WindowCycleListTestApi;
  friend class WindowCycleControllerTest;

  // Returns true if the given `window` is in a snap group and we need to step
  // twice to get to the next window cycle item.
  bool ShouldDoubleCycleStep(aura::Window* window,
                             WindowCyclingDirection direction) const;

  // aura::WindowObserver:
  // There is a chance a window is destroyed, for example by JS code. We need to
  // take care of that even if it is not intended for the user to close a window
  // while window cycling.
  void OnWindowDestroying(aura::Window* window) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Removes all windows from the window list. Also removes the windows from
  // |cycle_view_| if |cycle_view_| exists.
  void RemoveAllWindows();

  // Initializes and shows |cycle_view_|.
  void InitWindowCycleView();

  // Selects a window, which either activates it or expands it in the case of
  // PIP.
  void SelectWindow(aura::Window* window);

  // Scrolls windows by |offset|. Does not move the focus ring. If you want to
  // scroll the list and move the focus ring in one animation, call
  // SetFocusedWindow() before this.
  void Scroll(int offset);

  // Removes windows from `windows_` if they don't have the same app id as the
  // MRU window.
  void MakeSameAppOnly();

  // Returns the index for the window |offset| away from |current_index_|. Can
  // only be called if |windows_| is not empty. Also checks that the window for
  // the returned index exists.
  int GetOffsettedWindowIndex(int offset) const;

  // Returns the index for |window| in |windows_|. |window| must be in
  // |windows_|.
  int GetIndexOfWindow(aura::Window* window) const;

  // Returns the number of windows in the window cycle list for all desks.
  int GetNumberOfWindowsAllDesks() const;

  // Computes and reports the number of non-same-app windows skipped metric if
  // `same_app_only_`. This must be called from the destructor before the call
  // to `SelectWindow()` as it relies on the previous state of the MRU list.
  void MaybeReportNonSameAppSkippedWindows(aura::Window* target_window) const;

  // List of weak pointers to windows to use while cycling with the keyboard.
  // List is built when the user initiates the gesture (i.e. hits alt-tab the
  // first time) and is emptied when the gesture is complete (i.e. releases the
  // alt key).
  WindowList windows_;

  // Current position in the |windows_|. Can be used to query selection depth,
  // i.e., the position of an active window in a global MRU ordering.
  int current_index_ = 0;

  // True if the user accepted the window switch (as opposed to cancelling or
  // interrupting the interaction).
  bool user_did_accept_ = false;

  // True if one of the windows in the list has already been selected.
  bool window_selected_ = false;

  // True if we are only cycling through windows of the same app.
  const bool same_app_only_;

  // The top level View for the window cycle UI. May be null if the UI is not
  // showing.
  raw_ptr<WindowCycleView> cycle_view_ = nullptr;

  // The widget that hosts the window cycle UI.
  raw_ptr<views::Widget> cycle_ui_widget_ = nullptr;

  // The window list will dismiss if the display metrics change.
  display::ScopedDisplayObserver display_observer_{this};

  // A timer to delay showing the UI. Quick Alt+Tab should not flash a UI.
  base::OneShotTimer show_ui_timer_;

  // This is needed so that it won't leak keyboard events even if the widget is
  // not activatable.
  std::unique_ptr<aura::ScopedWindowTargeter> window_targeter_;

  // Tracks what window was active when starting to cycle and used to determine
  // if alt-tab should focus the first or the second window in the list.
  raw_ptr<aura::Window> active_window_before_window_cycle_ = nullptr;

  // The most recent direction `Step()` was called with.
  WindowCyclingDirection last_cycling_direction_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_WINDOW_CYCLE_LIST_H_
