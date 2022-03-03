// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_LAYER_ANIMATION_STOPPED_WAITER_H_
#define ASH_TEST_LAYER_ANIMATION_STOPPED_WAITER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"

namespace base {
class RunLoop;
}

namespace ui {
class Layer;
class LayerAnimator;
}  // namespace ui

namespace ash {

// A class capable of waiting until a layer has stopped animating. Supports
// animations that delete the layer on completion.
class LayerAnimationStoppedWaiter : public ui::LayerAnimationObserver {
 public:
  LayerAnimationStoppedWaiter();
  LayerAnimationStoppedWaiter(const LayerAnimationStoppedWaiter&) = delete;
  LayerAnimationStoppedWaiter& operator=(const LayerAnimationStoppedWaiter&) =
      delete;
  ~LayerAnimationStoppedWaiter() override;

  // Waits until the specified `layer`'s animation is stopped.
  void Wait(ui::Layer* layer);

 private:
  // ui::LayerAnimationObserver:
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override {}

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;

  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;

  ui::LayerAnimator* layer_animator_ = nullptr;
  base::ScopedObservation<ui::LayerAnimator, ui::LayerAnimationObserver>
      layer_animator_observer_{this};
  std::unique_ptr<base::RunLoop> wait_loop_;
};

}  // namespace ash

#endif  // ASH_TEST_LAYER_ANIMATION_STOPPED_WAITER_H_
