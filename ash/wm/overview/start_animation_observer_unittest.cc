// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/start_animation_observer.h"

#include <vector>

#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "base/containers/unique_ptr_adapters.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform.h"

namespace ash {

namespace {

class TestWindowSelectorDelegate : public WindowSelectorDelegate {
 public:
  TestWindowSelectorDelegate() = default;

  ~TestWindowSelectorDelegate() override = default;

  // WindowSelectorDelegate:
  void OnSelectionEnded() override {}
  void AddDelayedAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) override {}
  void RemoveAndDestroyAnimationObserver(
      DelayedAnimationObserver* animation_observer) override {}
  void AddStartAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) override {
    animation_observer->SetOwner(this);
    observers_.push_back(std::move(animation_observer));
  }
  void RemoveAndDestroyStartAnimationObserver(
      DelayedAnimationObserver* animation_observer) override {
    base::EraseIf(observers_, base::MatchesUniquePtr(animation_observer));
  }

  size_t NumObservers() const { return observers_.size(); }

 private:
  std::vector<std::unique_ptr<DelayedAnimationObserver>> observers_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowSelectorDelegate);
};

}  // namespace

using StartAnimationObserverTest = AshTestBase;

// Tests that adding a StartAnimationObserver works as intended.
TEST_F(StartAnimationObserverTest, Basic) {
  TestWindowSelectorDelegate delegate;
  std::unique_ptr<aura::Window> window = CreateTestWindow();

  {
    ui::ScopedLayerAnimationSettings animation_settings(
        window->layer()->GetAnimator());
    animation_settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(1000));
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    auto observer = std::make_unique<StartAnimationObserver>();
    animation_settings.AddObserver(observer.get());
    delegate.AddStartAnimationObserver(std::move(observer));
    window->SetTransform(gfx::Transform(1.f, 0.f, 0.f, 1.f, 100.f, 0.f));
    EXPECT_EQ(1u, delegate.NumObservers());
  }

  // Tests that when done animating, the observer count is zero.
  window->layer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(0u, delegate.NumObservers());
}

}  // namespace ash