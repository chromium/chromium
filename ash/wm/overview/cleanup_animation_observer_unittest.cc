// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/cleanup_animation_observer.h"

#include <vector>

#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "base/containers/unique_ptr_adapters.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
namespace {

class TestWindowSelectorDelegate : public WindowSelectorDelegate {
 public:
  TestWindowSelectorDelegate() = default;

  ~TestWindowSelectorDelegate() override {
    // Destroy widgets that may be still animating if shell shuts down soon
    // after exiting overview mode.
    for (std::unique_ptr<DelayedAnimationObserver>& observer : observers_)
      observer->Shutdown();
  }

  // WindowSelectorDelegate:
  void OnSelectionEnded() override {}

  void AddDelayedAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) override {
    animation_observer->SetOwner(this);
    observers_.push_back(std::move(animation_observer));
  }

  void RemoveAndDestroyAnimationObserver(
      DelayedAnimationObserver* animation_observer) override {
    base::EraseIf(observers_, base::MatchesUniquePtr(animation_observer));
  }

  void AddStartAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) override {}
  void RemoveAndDestroyStartAnimationObserver(
      DelayedAnimationObserver* animation_observer) override {}

 private:
  std::vector<std::unique_ptr<DelayedAnimationObserver>> observers_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowSelectorDelegate);
};

class CleanupAnimationObserverTest : public AshTestBase,
                                     public views::WidgetObserver {
 public:
  CleanupAnimationObserverTest() = default;

  ~CleanupAnimationObserverTest() override {
    if (widget_)
      widget_->RemoveObserver(this);
  }

  // Creates a Widget containing a Window with the given |bounds|. This should
  // be used when the test requires a Widget. For example any test that will
  // cause a window to be closed via
  // views::Widget::GetWidgetForNativeView(window)->Close().
  std::unique_ptr<views::Widget> CreateWindowWidget(const gfx::Rect& bounds) {
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params;
    params.bounds = bounds;
    params.type = views::Widget::InitParams::TYPE_WINDOW;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = CurrentContext();
    widget->Init(params);
    widget->Show();
    widget->AddObserver(this);
    widget_ = widget.get();
    return widget;
  }

 protected:
  bool widget_destroyed() { return !widget_; }

 private:
  void OnWidgetDestroyed(views::Widget* widget) override {
    if (widget_ == widget)
      widget_ = nullptr;
  }

  views::Widget* widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CleanupAnimationObserverTest);
};

}  // namespace

// Tests that basic create-destroy sequence does not crash.
TEST_F(CleanupAnimationObserverTest, CreateDestroy) {
  TestWindowSelectorDelegate delegate;
  std::unique_ptr<views::Widget> widget = CreateWindowWidget(gfx::Rect(40, 40));
  auto observer = std::make_unique<CleanupAnimationObserver>(std::move(widget));
  delegate.AddDelayedAnimationObserver(std::move(observer));
}

// Tests that completing animation deletes the animation observer and the
// test widget and that deleting the WindowSelectorDelegate instance which
// owns the observer does not crash.
TEST_F(CleanupAnimationObserverTest, CreateAnimateComplete) {
  TestWindowSelectorDelegate delegate;
  std::unique_ptr<views::Widget> widget = CreateWindowWidget(gfx::Rect(40, 40));
  aura::Window* widget_window = widget->GetNativeWindow();
  {
    ui::ScopedLayerAnimationSettings animation_settings(
        widget_window->layer()->GetAnimator());
    animation_settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(1000));
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    auto observer =
        std::make_unique<CleanupAnimationObserver>(std::move(widget));
    animation_settings.AddObserver(observer.get());
    delegate.AddDelayedAnimationObserver(std::move(observer));

    widget_window->SetBounds(gfx::Rect(50, 50, 60, 60));
  }
  // The widget should be destroyed when |animation_settings| gets out of scope
  // which in absence of NON_ZERO_DURATION animation duration mode completes
  // the animation and calls OnImplicitAnimationsCompleted() on the cleanup
  // observer and auto-deletes the owned widget.
  EXPECT_TRUE(widget_destroyed());
  // TestWindowSelectorDelegate going out of scope should not crash.
}

// Tests that starting an animation and exiting doesn't crash. If not for
// TestWindowSelectorDelegate calling Shutdown() on a CleanupAnimationObserver
// instance in destructor, this test would have crashed.
TEST_F(CleanupAnimationObserverTest, CreateAnimateShutdown) {
  TestWindowSelectorDelegate delegate;
  std::unique_ptr<views::Widget> widget = CreateWindowWidget(gfx::Rect(40, 40));
  aura::Window* widget_window = widget->GetNativeWindow();
  {
    // Normal animations for tests have ZERO_DURATION, make sure we are actually
    // animating the movement.
    ui::ScopedAnimationDurationScaleMode animation_scale_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    ui::ScopedLayerAnimationSettings animation_settings(
        widget_window->layer()->GetAnimator());
    animation_settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(1000));
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    auto observer =
        std::make_unique<CleanupAnimationObserver>(std::move(widget));
    animation_settings.AddObserver(observer.get());
    delegate.AddDelayedAnimationObserver(std::move(observer));

    widget_window->SetBounds(gfx::Rect(50, 50, 60, 60));
  }
  // The widget still exists.
  EXPECT_FALSE(widget_destroyed());
  // The test widget is auto-deleted when |delegate| that owns it goes out of
  // scope. The animation is still active when this happens which should not
  // crash.
}

}  // namespace ash
