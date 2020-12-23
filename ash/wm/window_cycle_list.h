// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_LIST_H_
#define ASH_WM_WINDOW_CYCLE_LIST_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/window_cycle_controller.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/views/view.h"

namespace aura {
class ScopedWindowTargeter;
class Window;
}

namespace views {
class Widget;
}

namespace ash {

class WindowCycleView;

// Tracks a set of Windows that can be stepped through. This class is used by
// the WindowCycleController.
class ASH_EXPORT WindowCycleList : public aura::WindowObserver,
                                   public display::DisplayObserver {
 public:
  using WindowList = std::vector<aura::Window*>;

  explicit WindowCycleList(const WindowList& windows);
  WindowCycleList(const WindowCycleList&) = delete;
  WindowCycleList& operator=(const WindowCycleList&) = delete;
  ~WindowCycleList() override;

  // Removes the existing windows and replaces them with |windows|. If
  // |windows| is empty, cancels cycling.
  void ReplaceWindows(const WindowList& windows);

  // Cycles to the next or previous window based on |direction|. This moves the
  // focus ring to the next/previous window and also scrolls the list.
  void Step(WindowCycleController::Direction direction);

  // Scrolls windows in given |direction|. Does not move the focus ring.
  void ScrollInDirection(WindowCycleController::Direction direction);

  // Moves the focus ring to the respective preview for |window|. Does not
  // scroll the window cycle list.
  void SetFocusedWindow(aura::Window* window);

  // Checks whether |event| occurs within the cycle view. Returns false if
  // |cycle_view_| does not exist.
  bool IsEventInCycleView(ui::LocatedEvent* event);

  // Returns true if the window list overlay should be shown.
  bool ShouldShowUi();

  void set_user_did_accept(bool user_did_accept) {
    user_did_accept_ = user_did_accept;
  }

 private:
  friend class WindowCycleControllerTest;
  friend class InteractiveWindowCycleListGestureHandlerTest;

  static void DisableInitialDelayForTesting();

  const WindowList& windows() const { return windows_; }
  const views::Widget* widget() const { return cycle_ui_widget_; }

  // aura::WindowObserver overrides:
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

  // Returns the index for the window |offset| away from |current_index_|. Can
  // only be called if |windows_| is not empty. Also checks that the window for
  // the returned index exists.
  int GetOffsettedWindowIndex(int offset) const;

  // Returns the index for |window| in |windows_|. |window| must be in
  // |windows_|.
  int GetIndexOfWindow(aura::Window* window) const;

  // Returns the views for the window cycle list.
  const views::View::Views& GetWindowCycleItemViewsForTesting() const;

  // Returns the window cycle list's target window.
  const aura::Window* GetTargetWindowForTesting() const;

  WindowCycleView* cycle_view_for_testing() const { return cycle_view_; }

  int current_index_for_testing() const { return current_index_; }

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

  // The top level View for the window cycle UI. May be null if the UI is not
  // showing.
  WindowCycleView* cycle_view_ = nullptr;

  // The widget that hosts the window cycle UI.
  views::Widget* cycle_ui_widget_ = nullptr;

  // The window list will dismiss if the display metrics change.
  ScopedObserver<display::Screen, display::DisplayObserver> screen_observer_{
      this};

  // A timer to delay showing the UI. Quick Alt+Tab should not flash a UI.
  base::OneShotTimer show_ui_timer_;

  // This is needed so that it won't leak keyboard events even if the widget is
  // not activatable.
  std::unique_ptr<aura::ScopedWindowTargeter> window_targeter_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_LIST_H_
