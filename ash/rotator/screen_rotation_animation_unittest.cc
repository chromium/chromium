// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation_animation.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform.h"

namespace ash {

class ScreenRotationAnimationTest : public AshTestBase {
 public:
  ScreenRotationAnimationTest() = default;

  ScreenRotationAnimationTest(const ScreenRotationAnimationTest&) = delete;
  ScreenRotationAnimationTest& operator=(const ScreenRotationAnimationTest&) =
      delete;

  ~ScreenRotationAnimationTest() override = default;

  // AshTestBase:
  void SetUp() override;

 private:
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> non_zero_duration_mode_;
};

void ScreenRotationAnimationTest::SetUp() {
  AshTestBase::SetUp();
  non_zero_duration_mode_ =
      std::make_unique<ui::ScopedAnimationDurationScaleMode>(
          ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
}

TEST_F(ScreenRotationAnimationTest, LayerTransformGetsSetToTargetWhenAborted) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(9));
  ui::Layer* layer = window->layer();

  std::unique_ptr<ScreenRotationAnimation> screen_rotation =
      std::make_unique<ScreenRotationAnimation>(
          layer, 45 /* start_degrees */, 0 /* end_degrees */,
          0.5f /* initial_opacity */, 1.0f /* target_opacity */,
          gfx::Point(10, 10) /* pivot */, base::Seconds(10) /* duration */,
          gfx::Tween::LINEAR);

  ui::LayerAnimator* animator = layer->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  std::unique_ptr<ui::LayerAnimationSequence> animation_sequence =
      std::make_unique<ui::LayerAnimationSequence>(std::move(screen_rotation));
  animator->StartAnimation(animation_sequence.release());

  const gfx::Transform identity_transform;

  ASSERT_EQ(identity_transform, layer->GetTargetTransform());
  ASSERT_NE(identity_transform, layer->transform());

  layer->GetAnimator()->AbortAllAnimations();

  EXPECT_EQ(identity_transform, layer->transform());
}

// Tests that ScreenRotationAnimation::OnAbort() doesn't segfault when passed a
// null delegate in response to its ui::Layer being destroyed:
// http://crbug.com/661313
TEST_F(ScreenRotationAnimationTest, DestroyLayerDuringAnimation) {
  // Create a ui::Layer directly rather than an aura::Window, as the latter
  // finishes all of its animation before destroying its layer.
  std::unique_ptr<ui::Layer> layer = std::make_unique<ui::Layer>();

  ui::Layer* root_layer = GetContext()->layer();
  layer->SetBounds(gfx::Rect(root_layer->bounds().size()));
  root_layer->Add(layer.get());

  std::unique_ptr<ScreenRotationAnimation> screen_rotation =
      std::make_unique<ScreenRotationAnimation>(layer.get(), 45, 0, 1.0f, 1.0f,
                                                gfx::Point(), base::Seconds(1),
                                                gfx::Tween::LINEAR);
  ui::LayerAnimator* animator = layer->GetAnimator();
  std::unique_ptr<ui::LayerAnimationSequence> animation_sequence =
      std::make_unique<ui::LayerAnimationSequence>(std::move(screen_rotation));
  animator->StartAnimation(animation_sequence.release());

  // Explicitly destroy the layer to verify that the animation doesn't crash.
  layer.reset();
}

}  // namespace ash
