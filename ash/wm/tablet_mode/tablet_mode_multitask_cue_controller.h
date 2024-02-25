// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_CUE_CONTROLLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_CUE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/window_state_observer.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

// Creates a cue (draggable bar) at the top center of an app window when it is
// activated in tablet mode. Only one cue exists at a time.
class ASH_EXPORT TabletModeMultitaskCueController
    : aura::WindowObserver,
      wm::ActivationChangeObserver,
      WindowStateObserver {
 public:
  // Cue layout values.
  static constexpr int kCueYOffset = 6;
  static constexpr int kCueWidth = 48;
  static constexpr int kCueHeight = 4;

  TabletModeMultitaskCueController();

  TabletModeMultitaskCueController(const TabletModeMultitaskCueController&) =
      delete;
  TabletModeMultitaskCueController& operator=(
      const TabletModeMultitaskCueController&) = delete;

  ~TabletModeMultitaskCueController() override;

  ui::Layer* cue_layer() { return cue_layer_.get(); }

  // Shows the cue if `active_window` is an maximizable app window that is not
  // floated. Also sets a `OneShotTimer` to dismiss the cue after a short
  // duration.
  void MaybeShowCue(aura::Window* active_window);

  // Returns false if the cue cannot be shown on `window` (e.g., non-app
  // windows), and true otherwise.
  bool CanShowCue(aura::Window* window) const;

  // Dismisses the cue from the screen and cleans up the pointers and
  // observers related to its parent window.
  void DismissCue();

  // If the cue is visible, checks to see if it is on the same window as the
  // multitask menu, and shows it on the correct window if not.
  void OnMenuOpened(aura::Window* active_window);

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

  void FireCueDismissTimerForTesting() { cue_dismiss_timer_.FireNow(); }
  chromeos::MultitaskMenuNudgeController* nudge_controller_for_testing() {
    return &nudge_controller_;
  }

  void set_pre_cue_shown_callback_for_test(base::OnceClosure callback) {
    pre_cue_shown_callback_for_test_ = std::move(callback);
  }

 private:
  friend class TabletModeMultitaskMenu;
  FRIEND_TEST_ALL_PREFIXES(TabletModeMultitaskMenuTest,
                           CueCorrectWindowInSplitView);

  // Updates the bounds of the cue relative to the window if the window is
  // still available.
  void UpdateCueBounds();

  // Fades the cue out over a short duration if it is still active, then cleans
  // up via `DismissCue`. If already fading out, returns immediately.
  void OnTimerFinished();

  // The app window that the cue is associated with.
  raw_ptr<aura::Window> window_ = nullptr;

  // Handles showing the educational nudge for the tablet multitask menu.
  chromeos::MultitaskMenuNudgeController nudge_controller_;

  // The solid color layer that represents the cue.
  std::unique_ptr<ui::Layer> cue_layer_;

  // Observes for window destruction or bounds changes.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};

  // Dismisses the cue after a short amount of time if it is still active.
  base::OneShotTimer cue_dismiss_timer_;

  // If set, will be called after all checks have been passed but before the cue
  // is initialized.
  base::OnceClosure pre_cue_shown_callback_for_test_;
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_CUE_CONTROLLER_H_
