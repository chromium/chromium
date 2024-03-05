// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/delayed_animation_observer_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_delegate.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/test/task_environment.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform.h"

namespace ash {

namespace {

class TestOverviewDelegate : public OverviewDelegate {
 public:
  TestOverviewDelegate() = default;

  TestOverviewDelegate(const TestOverviewDelegate&) = delete;
  TestOverviewDelegate& operator=(const TestOverviewDelegate&) = delete;

  ~TestOverviewDelegate() override = default;

  // OverviewDelegate:
  void AddExitAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) override {
    animation_observer->SetOwner(this);
    exit_observers_.push_back(std::move(animation_observer));
  }
  void RemoveAndDestroyExitAnimationObserver(
      DelayedAnimationObserver* animation_observer) override {
    std::erase_if(exit_observers_, base::MatchesUniquePtr(animation_observer));
  }
  void AddEnterAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) override {
    animation_observer->SetOwner(this);
    enter_observers_.push_back(std::move(animation_observer));
  }
  void RemoveAndDestroyEnterAnimationObserver(
      DelayedAnimationObserver* animation_observer) override {
    std::erase_if(enter_observers_, base::MatchesUniquePtr(animation_observer));
  }

  size_t num_exit_observers() const { return exit_observers_.size(); }
  size_t num_enter_observers() const { return enter_observers_.size(); }

 private:
  std::vector<std::unique_ptr<DelayedAnimationObserver>> exit_observers_;
  std::vector<std::unique_ptr<DelayedAnimationObserver>> enter_observers_;
};

}  // namespace

class ForceDelayObserverTest : public AshTestBase {
 public:
  ForceDelayObserverTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ForceDelayObserverTest(const ForceDelayObserverTest&) = delete;
  ForceDelayObserverTest& operator=(const ForceDelayObserverTest&) = delete;

  ~ForceDelayObserverTest() override = default;
};

TEST_F(ForceDelayObserverTest, Basic) {
  TestOverviewDelegate delegate;

  auto observer = std::make_unique<ForceDelayObserver>(base::Milliseconds(100));
  delegate.AddEnterAnimationObserver(std::move(observer));
  EXPECT_EQ(1u, delegate.num_enter_observers());

  task_environment()->FastForwardBy(base::Milliseconds(50));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, delegate.num_enter_observers());
  task_environment()->FastForwardBy(base::Milliseconds(55));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, delegate.num_enter_observers());
}

using EnterAnimationObserverTest = AshTestBase;

// Tests that adding a EnterAnimationObserver works as intended.
TEST_F(EnterAnimationObserverTest, Basic) {
  TestOverviewDelegate delegate;
  std::unique_ptr<aura::Window> window = CreateTestWindow();

  {
    ui::ScopedLayerAnimationSettings animation_settings(
        window->layer()->GetAnimator());
    animation_settings.SetTransitionDuration(base::Milliseconds(1000));
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    auto observer = std::make_unique<EnterAnimationObserver>();
    animation_settings.AddObserver(observer.get());
    delegate.AddEnterAnimationObserver(std::move(observer));
    window->SetTransform(gfx::Transform::MakeTranslation(100.f, 0.f));
    EXPECT_EQ(0u, delegate.num_exit_observers());
    EXPECT_EQ(1u, delegate.num_enter_observers());
  }

  // Tests that when done animating, the observer count is zero.
  window->layer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(0u, delegate.num_enter_observers());
}

using ExitAnimationObserverTest = AshTestBase;

// Tests that adding a ExitAnimationObserver works as intended.
TEST_F(ExitAnimationObserverTest, Basic) {
  TestOverviewDelegate delegate;
  std::unique_ptr<aura::Window> window = CreateTestWindow();

  {
    ui::ScopedLayerAnimationSettings animation_settings(
        window->layer()->GetAnimator());
    animation_settings.SetTransitionDuration(base::Milliseconds(1000));
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    auto observer = std::make_unique<ExitAnimationObserver>();
    animation_settings.AddObserver(observer.get());
    delegate.AddExitAnimationObserver(std::move(observer));
    window->SetTransform(gfx::Transform::MakeTranslation(100.f, 0.f));
    EXPECT_EQ(1u, delegate.num_exit_observers());
    EXPECT_EQ(0u, delegate.num_enter_observers());
  }

  // Tests that when done animating, the observer count is zero.
  window->layer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(0u, delegate.num_exit_observers());
}

}  // namespace ash
