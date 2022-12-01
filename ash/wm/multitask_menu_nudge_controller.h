// Copyright 2022 The Chromium Authors
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

namespace ui {
class Layer;
}

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
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowTargetTransformChanging(
      aura::Window* window,
      const gfx::Transform& new_transform) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  ASH_EXPORT static void SetSuppressNudgeForTesting(bool val);

 private:
  friend class MultitaskMenuNudgeControllerTest;

  // Used to control the clock in a test setting.
  ASH_EXPORT static void SetOverrideClockForTesting(base::Clock* test_clock);

  // Runs when the nudge dismiss timer expires. Dismisses the nudge if it is
  // being shown.
  void OnDismissTimerEnded();

  // Closes the widget and cleans up all pointers and observers in this class.
  void DismissNudgeInternal();

  // Dismisses the widget and pulse if `anchor_view` is not drawn, or `window`
  // is not visible. Otherwise updates the bounds and reparents the two if
  // necessary.
  void UpdateWidgetAndPulse();

  // The animation associated with `pulse_layer_`. Runs until `pulse_layer_` is
  // destroyed or `pulse_count` reaches 3.
  void PerformPulseAnimation(int pulse_count);

  base::OneShotTimer nudge_dismiss_timer_;

  views::UniqueWidgetPtr nudge_widget_;
  std::unique_ptr<ui::Layer> pulse_layer_;

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
