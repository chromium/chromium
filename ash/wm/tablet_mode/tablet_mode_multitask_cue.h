// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_CUE_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_CUE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/window_state_observer.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

// Creates a cue (drag bar) when app windows are activated in tablet mode.
class ASH_EXPORT TabletModeMultitaskCue : aura::WindowObserver,
                                          wm::ActivationChangeObserver,
                                          WindowStateObserver {
 public:
  TabletModeMultitaskCue();

  TabletModeMultitaskCue(const TabletModeMultitaskCue&) = delete;
  TabletModeMultitaskCue& operator=(const TabletModeMultitaskCue&) = delete;

  ~TabletModeMultitaskCue() override;

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

  ui::Layer* cue_layer_for_testing() { return cue_layer_.get(); }

 private:
  // Dismisses the cue from the screen and cleans up the pointers and
  // observers related to its parent window.
  void DismissCueInternal();

  // Updates the bounds of the cue relative to the window if the window is
  // still available.
  void UpdateCueBounds();

  // The app window that the cue is associated with.
  aura::Window* window_ = nullptr;

  // The solid color layer that represents the cue (drag bar).
  std::unique_ptr<ui::Layer> cue_layer_;

  // Observes for window destruction or bounds changes.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_MULTITASK_CUE_H_