// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_FLOAT_CONTAINER_STACKER_H_
#define ASH_WM_OVERVIEW_SCOPED_FLOAT_CONTAINER_STACKER_H_

#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/callback_layer_animation_observer.h"

namespace ash {

class OverviewWindowDragController;

// Helps with handling the workflow when you drag an overview item and there is
// a floated window. Floated windows are in a higher z-order container, so
// dragging the item would normally go under the floated window. This helper
// handles stacking the float container below the desk containers during the
// drag, and restoring it after dragging is finished and the window animation is
// complete, or overview ends.
class ScopedFloatContainerStacker : public aura::WindowObserver {
 public:
  explicit ScopedFloatContainerStacker(OverviewWindowDragController* owner);
  ScopedFloatContainerStacker(const ScopedFloatContainerStacker&) = delete;
  ScopedFloatContainerStacker& operator=(const ScopedFloatContainerStacker&) =
      delete;
  ~ScopedFloatContainerStacker() override;

  // Called when a gesture is completed or canceled. Preferred over directly
  // destroying this object as this handles the case where the window is
  // animating.
  void Shutdown(aura::Window* dragged_window);

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Callback run by `animation_observer_`.
  bool OnAnimationsCompleted(
      const ui::CallbackLayerAnimationObserver& observer);

  OverviewWindowDragController* const owner_;

  // Not null when a dragged window has been released and is animating to its
  // final position.
  aura::Window* dragged_window_ = nullptr;
  std::unique_ptr<ui::CallbackLayerAnimationObserver> animation_observer_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      dragged_window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_FLOAT_CONTAINER_STACKER_H_
