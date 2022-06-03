// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MULTITASK_MENU_NUDGE_CONTROLLER_H_
#define ASH_WM_MULTITASK_MENU_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

class PrefRegistrySimple;

namespace ash {

// Controller for showing the user education nudge for the multitask menu.
// TODO(sammiequon|shidi): This will be extended for the multitask menu in
// tablet mode too once that is implemented.
class MultitaskMenuNudgeController : public aura::WindowObserver {
 public:
  MultitaskMenuNudgeController();
  MultitaskMenuNudgeController(const MultitaskMenuNudgeController&) = delete;
  MultitaskMenuNudgeController& operator=(const MultitaskMenuNudgeController&) =
      delete;
  ~MultitaskMenuNudgeController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Shows the nudge if it can be shown. The nudge can be shown if it hasn't
  // been shown 3 times already, or shown in the last 24 hours.
  void MaybeShowNudge(aura::Window* window);

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

 private:
  friend class MultitaskMenuNudgeControllerTest;

  // Used to control the clock in a test setting.
  ASH_EXPORT static void SetOverrideClockForTesting(base::Clock* test_clock);

  // Runs when the nudge dismiss timer expires. Dismisses the nudge if it is
  // being shown.
  void OnDismissTimerEnded();

  // Closes the widget and cleans up all pointers and observers in this class.
  void DismissNudgeInternal();

  // Updates the widget so that it is underneath the `anchor_view`.
  void UpdateWidgetBounds();

  views::UniqueWidgetPtr nudge_widget_;

  base::OneShotTimer nudge_dismiss_timer_;

  // The app window that the nudge is associated with. It is expected to have a
  // header with a maximize/restore button.
  aura::Window* window_ = nullptr;
  // The view that the nudge will be anchored to. It is the maximize or resize
  // button on `window_`'s frame.
  views::View* anchor_view_ = nullptr;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_MULTITASK_MENU_NUDGE_CONTROLLER_H_
