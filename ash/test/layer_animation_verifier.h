// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_LAYER_ANIMATION_VERIFIER_H_
#define ASH_TEST_LAYER_ANIMATION_VERIFIER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class Layer;
}

namespace views {
class View;
}

namespace ash {

// The helper class to verify a layer animation on the following things:
// (1) The observed view should not go back and forth during the animation.
// (2) TODO(crbug.com/40181947): The animation should progress to the end
// rather than get interrupted.
// (3) TODO(https://crbug.com/1209001): The observed view should move smoothly
// during the animation. In other words, the view should not move in a janky
// manner (such as jumping).
class LayerAnimationVerifier : public ui::CompositorObserver {
 public:
  // `layer_with_animation` and `observed_view` should share the compositor.
  // Otherwise it is unmeaningful to watch `observed_view` during animation.
  LayerAnimationVerifier(ui::Layer* layer_with_animation,
                         views::View* observed_view);
  LayerAnimationVerifier(const LayerAnimationVerifier&) = delete;
  LayerAnimationVerifier& operator=(const LayerAnimationVerifier&) = delete;
  ~LayerAnimationVerifier() override;

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override;

 private:
  // Indicates move direction during animation.
  enum class MoveDirection { kForward, kBackward };

  // Compares `current_screen_bounds` with the most recent screen bounds.
  void CompareWithLastBoundsRect(const gfx::Rect& current_screen_bounds);

  // Calculates the move direction.
  MoveDirection CalculateMoveDirection(int current_data, int last_data) const;

  // Returns the compositor of `layer_with_animation`.
  ui::Compositor* GetCompositor();

  // Indicates the layer on which an animation is going to apply.
  const raw_ptr<ui::Layer> layer_with_animation_;

  // Indicates the view that is observed during the layer animation.
  const raw_ptr<const views::View> observed_view_;

  // The screen bounds of `observed_view_` in the most recent compositor commit.
  std::optional<gfx::Rect> last_screen_bounds_;

  // `observed_view_`'s move direction on x-axis and y-axis respectively.
  std::optional<MoveDirection> x_direction_;
  std::optional<MoveDirection> y_direction_;

  // Indicates the number of comparisons during animation.
  int comparison_count_ = 0;
};

}  // namespace ash

#endif  // ASH_TEST_LAYER_ANIMATION_VERIFIER_H_
