// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_FLOAT_CONTAINER_STACKER_H_
#define ASH_WM_OVERVIEW_SCOPED_FLOAT_CONTAINER_STACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/callback_layer_animation_observer.h"

namespace ash {

// Helps with handling the workflow when you drag an overview item and there is
// a floated window. Floated windows are in a higher z-order container, so
// dragging the item would normally go under the floated window. This helper
// handles stacking the float container below the desk containers during the
// drag, and restoring it after dragging is finished and the window animation is
// complete, or overview ends.
class ScopedFloatContainerStacker : public aura::WindowObserver {
 public:
  ScopedFloatContainerStacker();
  ScopedFloatContainerStacker(const ScopedFloatContainerStacker&) = delete;
  ScopedFloatContainerStacker& operator=(const ScopedFloatContainerStacker&) =
      delete;
  ~ScopedFloatContainerStacker() override;

  // Stacks the float container above the desk container if we are dragging a
  // floated window.
  void OnDragStarted(aura::Window* dragged_window);

  // If `dragged_window` is animating, creates the animation observer to restack
  // the float container on animation end, otherwise restacks the float
  // container immediately.
  void OnDragFinished(aura::Window* dragged_window);

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Callback run by `animation_observer_`.
  bool OnAnimationsCompleted(
      const ui::CallbackLayerAnimationObserver& observer);

  // Cleanups the stored members needed for observing an animation.
  void Cleanup();

  // Set to true during shutdown. This is to prevent the animation callback from
  // doing work if it is called by deleting the observer.
  bool is_destroying_ = false;

  // Not null when a dragged window has been released and is animating to its
  // final position.
  raw_ptr<aura::Window> dragged_window_ = nullptr;
  std::unique_ptr<ui::CallbackLayerAnimationObserver> animation_observer_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      dragged_window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_FLOAT_CONTAINER_STACKER_H_
