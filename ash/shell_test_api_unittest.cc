// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

namespace ash {
namespace {

// Recreates the layer of `window` and destroys the old layer, which owns the
// animator being observed, when an animation on the layer ends.
class LayerRecreator : public ui::LayerAnimationObserver {
 public:
  explicit LayerRecreator(aura::Window* window) : window_(window) {
    window_->layer()->GetAnimator()->AddObserver(this);
  }
  LayerRecreator(const LayerRecreator&) = delete;
  LayerRecreator& operator=(const LayerRecreator&) = delete;
  ~LayerRecreator() override = default;

  bool recreated() const { return recreated_; }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    window_->layer()->GetAnimator()->RemoveObserver(this);
    window_->RecreateLayer().reset();
    recreated_ = true;
  }
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

 private:
  const raw_ptr<aura::Window> window_;
  bool recreated_ = false;
};

using ShellTestApiTest = AshTestBase;

}  // namespace

// Tests that waiting for a window animation is safe if the window's layer is
// recreated and the old layer is destroyed when the animation ends.
TEST_F(ShellTestApiTest, WaitForWindowFinishAnimatingWithLayerRecreated) {
  std::unique_ptr<aura::Window> window = CreateToplevelTestWindow({100, 100});

  gfx::ScopedAnimationDurationScaleMode non_zero_duration(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  {
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(100));
    window->layer()->SetOpacity(0.5f);
  }
  ASSERT_TRUE(window->layer()->GetAnimator()->is_animating());

  // `recreator` is added before the waiter, so it is notified first.
  LayerRecreator recreator(window.get());
  ShellTestApi().WaitForWindowFinishAnimating(window.get());
  EXPECT_TRUE(recreator.recreated());
}

}  // namespace ash
