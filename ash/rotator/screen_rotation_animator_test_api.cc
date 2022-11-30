// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation_animator_test_api.h"

#include "ash/rotator/screen_rotation_animator.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"

namespace ash {

ScreenRotationAnimatorTestApi::ScreenRotationAnimatorTestApi(
    ScreenRotationAnimator* animator)
    : ui::test::MultiLayerAnimatorTestController(this), animator_(animator) {}

ScreenRotationAnimatorTestApi::~ScreenRotationAnimatorTestApi() = default;

void ScreenRotationAnimatorTestApi::DisableAnimationTimers() {
  animator_->set_disable_animation_timers_for_test(true);
}

std::vector<ui::LayerAnimator*>
ScreenRotationAnimatorTestApi::GetLayerAnimators() {
  std::vector<ui::LayerAnimator*> animators;
  if (animator_->old_layer_tree_owner_) {
    animators.insert(animators.end(),
                     animator_->old_layer_tree_owner_->root()->GetAnimator());
  }
  if (animator_->new_layer_tree_owner_) {
    animators.insert(animators.end(),
                     animator_->new_layer_tree_owner_->root()->GetAnimator());
  }
  return animators;
}

}  // namespace ash
