// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_LAYER_COPY_ANIMATOR_H_
#define ASH_UTILITY_LAYER_COPY_ANIMATOR_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ash {

// A utility class that first creates a copy of the layer tree, then runs
// animation. This is more performant if you want to apply animation to a
// window/layer tree, as it only apply animation to one layer.
class ASH_EXPORT LayerCopyAnimator : public aura::WindowObserver,
                                     public ui::LayerAnimationObserver {
 public:
  // Returns an associated LayerCopyAnimator, if any.
  static LayerCopyAnimator* Get(aura::Window* window);

  using AnimationCallback =
      base::OnceCallback<void(ui::Layer*, ui::LayerAnimationObserver*)>;

  // Creates a LayerCopyAnimator for 'window'. The instance created for this
  // window can later be obtained by 'LayerCopyAnimator::Get()'. Creating an
  // instance will start copying the layer, but animation won't start until
  // 'Start' is called. This will also destroy the current LayerCopyAnimator if
  // any.
  explicit LayerCopyAnimator(aura::Window* window);
  LayerCopyAnimator(const LayerCopyAnimator& animator) = delete;
  LayerCopyAnimator& operator=(const LayerCopyAnimator& animator) = delete;
  ~LayerCopyAnimator() override;

  // Request to start an animation. It'll start animation of the layer's copy is
  // is already available, or wait until the copy is made.
  void MaybeStartAnimation(ui::LayerAnimationObserver* observer,
                           AnimationCallback callback);

  // Called when a layer is copied. This is public to deal with the shutdown
  // scenario. This is virtual for testing purpose.
  virtual void OnLayerCopied(std::unique_ptr<ui::Layer> new_layer);

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* seq) override {}

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  bool animation_requested() const { return animation_requested_; }

  ui::Layer* copied_layer_for_test() { return copied_layer_.get(); }

 private:
  void RunAnimation();
  void FinishAndDelete(bool abort);
  void EnsureFakeSequence();
  void NotifyWithFakeSequence(bool abort);

  raw_ptr<aura::Window> window_;
  raw_ptr<ui::LayerAnimationObserver, DanglingUntriaged> observer_ = nullptr;
  AnimationCallback animation_callback_;

  std::unique_ptr<ui::Layer> copied_layer_;
  // A dummy sequence to keep AnimationSequence alive during copy.
  std::unique_ptr<ui::LayerAnimationSequence> fake_sequence_;
  ui::Layer full_layer_{ui::LAYER_SOLID_COLOR};
  bool fail_ = false;
  bool animation_requested_ = false;
  raw_ptr<ui::LayerAnimationSequence> last_sequence_ = nullptr;

  base::ScopedObservation<aura::Window, aura::WindowObserver> observation_{
      this};

  base::WeakPtrFactory<LayerCopyAnimator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_UTILITY_LAYER_COPY_ANIMATOR_H_
