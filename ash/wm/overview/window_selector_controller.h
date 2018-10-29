// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_CONTROLLER_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace ash {
class WindowSelector;
class WindowSelectorTest;

// Manages a window selector which displays an overview of all windows and
// allows selecting a window to activate it.
class ASH_EXPORT WindowSelectorController : public WindowSelectorDelegate {
 public:
  enum class AnimationCompleteReason {
    kCompleted,
    kCanceled,
  };

  WindowSelectorController();
  ~WindowSelectorController() override;

  // Amount of blur to apply on the wallpaper when we enter or exit overview
  // mode.
  static constexpr float kWallpaperBlurSigma = 10.f;
  static constexpr float kWallpaperClearBlurSigma = 0.f;

  // Returns true if selecting windows in an overview is enabled. This is false
  // at certain times, such as when the lock screen is visible.
  static bool CanSelect();

  // Attempts to toggle overview mode and returns true if successful (showing
  // overview would be unsuccessful if there are no windows to show). Depending
  // on |type| the enter/exit animation will look different.
  bool ToggleOverview(WindowSelector::EnterExitOverviewType type =
                          WindowSelector::EnterExitOverviewType::kNormal);

  // Returns true if window selection mode is active.
  bool IsSelecting() const;

  // Returns true if overview has been shutdown, but is still animating to the
  // end state ui.
  bool IsCompletingShutdownAnimations();

  // Moves the current selection by |increment| items. Positive values of
  // |increment| move the selection forward, negative values move it backward.
  void IncrementSelection(int increment);

  // Accepts current selection if any. Returns true if a selection was made,
  // false otherwise.
  bool AcceptSelection();

  // Called when the overview button tray has been long pressed. Enters
  // splitview mode if the active window is snappable. Also enters overview mode
  // if device is not currently in overview mode.
  void OnOverviewButtonTrayLongPressed(const gfx::Point& event_location);

  // Gets the windows list that are shown in the overview windows grids if the
  // overview mode is active for testing.
  std::vector<aura::Window*> GetWindowsListInOverviewGridsForTesting();

  bool is_shutting_down() const { return is_shutting_down_; }

  // WindowSelectorDelegate:
  void OnSelectionEnded() override;
  void AddDelayedAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation) override;
  void RemoveAndDestroyAnimationObserver(
      DelayedAnimationObserver* animation) override;
  void AddStartAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) override;
  void RemoveAndDestroyStartAnimationObserver(
      DelayedAnimationObserver* animation_observer) override;

  WindowSelector* window_selector() { return window_selector_.get(); }

 private:
  class OverviewBlurController;
  friend class WindowSelectorTest;
  FRIEND_TEST_ALL_PREFIXES(TabletModeControllerTest,
                           DisplayDisconnectionDuringOverview);
  FRIEND_TEST_ALL_PREFIXES(WindowSelectorTest, OverviewExitAnimationObserver);

  // There is no need to blur or unblur the wallpaper for tests.
  static void SetDoNotChangeWallpaperBlurForTests();

  // Dispatched when window selection begins.
  void OnSelectionStarted();

  // Collection of DelayedAnimationObserver objects that own widgets that may be
  // still animating after overview mode ends. If shell needs to shut down while
  // those animations are in progress, the animations are shut down and the
  // widgets destroyed.
  std::vector<std::unique_ptr<DelayedAnimationObserver>> delayed_animations_;
  // Collection of DelayedAnimationObserver objects. When this becomes empty,
  // notify shell that the starting animations have been completed.
  std::vector<std::unique_ptr<DelayedAnimationObserver>> start_animations_;

  std::unique_ptr<WindowSelector> window_selector_;
  base::Time last_selection_time_;

  // If we are in middle of ending overview mode.
  bool is_shutting_down_ = false;

  // Handles blurring of the wallpaper when entering or exiting overview mode.
  // Animates the blurring if necessary.
  std::unique_ptr<OverviewBlurController> overview_blur_controller_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorController);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_CONTROLLER_H_
