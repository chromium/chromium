// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

using aura::Window;
using ui::Layer;

namespace ash {

class WindowAnimationsTest : public AshTestBase {
 public:
  WindowAnimationsTest() = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowAnimationsTest);
};

// Listens to animation scheduled notifications. Remembers the transition
// duration of the first sequence.
class MinimizeAnimationObserver : public ui::LayerAnimationObserver {
 public:
  explicit MinimizeAnimationObserver(ui::LayerAnimator* animator)
      : animator_(animator) {
    animator_->AddObserver(this);
    // RemoveObserver is called when the first animation is scheduled and so
    // there should be no need for now to remove it in destructor.
  }
  base::TimeDelta duration() { return duration_; }

 protected:
  // ui::LayerAnimationObserver:
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {
    duration_ = animator_->GetTransitionDuration();
    animator_->RemoveObserver(this);
  }
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}

 private:
  ui::LayerAnimator* animator_;
  base::TimeDelta duration_;

  DISALLOW_COPY_AND_ASSIGN(MinimizeAnimationObserver);
};

TEST_F(WindowAnimationsTest, HideShowBrightnessGrayscaleAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  window->Show();
  EXPECT_TRUE(window->layer()->visible());

  // Hiding.
  wm::SetWindowVisibilityAnimationType(
      window.get(), WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE);
  AnimateOnChildWindowVisibilityChanged(window.get(), false);
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_FALSE(window->layer()->visible());

  // Showing.
  wm::SetWindowVisibilityAnimationType(
      window.get(), WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE);
  AnimateOnChildWindowVisibilityChanged(window.get(), true);
  EXPECT_EQ(0.0f, window->layer()->GetTargetBrightness());
  EXPECT_EQ(0.0f, window->layer()->GetTargetGrayscale());
  EXPECT_TRUE(window->layer()->visible());

  // Stays shown.
  window->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                       base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(0.0f, window->layer()->GetTargetBrightness());
  EXPECT_EQ(0.0f, window->layer()->GetTargetGrayscale());
  EXPECT_TRUE(window->layer()->visible());
}

TEST_F(WindowAnimationsTest, LayerTargetVisibility) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  // Layer target visibility changes according to Show/Hide.
  window->Show();
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  window->Hide();
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  window->Show();
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
}

TEST_F(WindowAnimationsTest, CrossFadeToBounds) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShellWithId(0));
  window->SetBounds(gfx::Rect(5, 10, 320, 240));
  window->Show();

  Layer* old_layer = window->layer();
  EXPECT_EQ(1.0f, old_layer->GetTargetOpacity());

  // Cross fade to a larger size, as in a maximize animation.
  WindowState::Get(window.get())
      ->SetBoundsDirectCrossFade(gfx::Rect(0, 0, 640, 480));
  // Window's layer has been replaced.
  EXPECT_NE(old_layer, window->layer());
  // Original layer stays opaque and stretches to new size.
  EXPECT_EQ(1.0f, old_layer->GetTargetOpacity());
  EXPECT_EQ("5,10 320x240", old_layer->bounds().ToString());
  gfx::Transform grow_transform;
  grow_transform.Translate(-5.f, -10.f);
  grow_transform.Scale(640.f / 320.f, 480.f / 240.f);
  EXPECT_EQ(grow_transform, old_layer->GetTargetTransform());
  // New layer animates in to the identity transform.
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(gfx::Transform(), window->layer()->GetTargetTransform());

  // Run the animations to completion.
  old_layer->GetAnimator()->Step(base::TimeTicks::Now() +
                                 base::TimeDelta::FromSeconds(1));
  window->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                       base::TimeDelta::FromSeconds(1));

  // Cross fade to a smaller size, as in a restore animation.
  old_layer = window->layer();
  WindowState::Get(window.get())
      ->SetBoundsDirectCrossFade(gfx::Rect(5, 10, 320, 240));
  // Again, window layer has been replaced.
  EXPECT_NE(old_layer, window->layer());
  // Original layer fades out and stretches down to new size.
  EXPECT_EQ(0.0f, old_layer->GetTargetOpacity());
  EXPECT_EQ("0,0 640x480", old_layer->bounds().ToString());
  gfx::Transform shrink_transform;
  shrink_transform.Translate(5.f, 10.f);
  shrink_transform.Scale(320.f / 640.f, 240.f / 480.f);
  EXPECT_EQ(shrink_transform, old_layer->GetTargetTransform());
  // New layer animates in to the identity transform.
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(gfx::Transform(), window->layer()->GetTargetTransform());

  old_layer->GetAnimator()->Step(base::TimeTicks::Now() +
                                 base::TimeDelta::FromSeconds(1));
  window->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                       base::TimeDelta::FromSeconds(1));
}

// Tests that when crossfading from a window which has a transform, the cross
// fading animation should be ignored and the window should set to its desired
// bounds directly.
TEST_F(WindowAnimationsTest, CrossFadeToBoundsFromTransform) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShellWithId(0));
  window->SetBounds(gfx::Rect(10, 10, 320, 240));
  gfx::Transform half_size;
  half_size.Translate(10, 10);
  half_size.Scale(0.5f, 0.5f);
  window->SetTransform(half_size);
  window->Show();

  Layer* old_layer = window->layer();
  EXPECT_EQ(1.0f, old_layer->GetTargetOpacity());

  // Cross fade to a larger size, as in a maximize animation.
  WindowState::Get(window.get())
      ->SetBoundsDirectCrossFade(gfx::Rect(0, 0, 640, 480));
  // Window's layer has not been replaced.
  EXPECT_EQ(old_layer, window->layer());
  // Original layer stays opaque and set to new size directly.
  EXPECT_EQ(1.0f, old_layer->GetTargetOpacity());
  EXPECT_EQ("0,0 640x480", old_layer->bounds().ToString());
  // Window still has its old transform before crossfading animation.
  EXPECT_EQ(half_size, old_layer->transform());
}

TEST_F(WindowAnimationsTest, LockAnimationDuration) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShellWithId(0));
  Layer* layer = window->layer();
  window->SetBounds(gfx::Rect(5, 10, 320, 240));
  window->Show();

  // Test that it is possible to override transition duration when it is not
  // locked.
  {
    ui::ScopedLayerAnimationSettings settings1(layer->GetAnimator());
    settings1.SetTransitionDuration(base::TimeDelta::FromMilliseconds(1000));
    {
      ui::ScopedLayerAnimationSettings settings2(layer->GetAnimator());
      // Duration is not locked so it gets overridden.
      settings2.SetTransitionDuration(base::TimeDelta::FromMilliseconds(50));
      WindowState::Get(window.get())->Minimize();
      EXPECT_TRUE(layer->GetAnimator()->is_animating());
      // Expect duration from the inner scope
      EXPECT_EQ(50,
                layer->GetAnimator()->GetTransitionDuration().InMilliseconds());
    }
    window->Show();
    layer->GetAnimator()->StopAnimating();
  }

  // Test that it is possible to lock transition duration
  {
    // Update layer as minimizing will replace the window's layer.
    layer = window->layer();
    ui::ScopedLayerAnimationSettings settings1(layer->GetAnimator());
    settings1.SetTransitionDuration(base::TimeDelta::FromMilliseconds(1000));
    // Duration is locked in outer scope.
    settings1.LockTransitionDuration();
    {
      ui::ScopedLayerAnimationSettings settings2(layer->GetAnimator());
      // Transition duration setting is ignored.
      settings2.SetTransitionDuration(base::TimeDelta::FromMilliseconds(50));
      WindowState::Get(window.get())->Minimize();
      EXPECT_TRUE(layer->GetAnimator()->is_animating());
      // Expect duration from the outer scope
      EXPECT_EQ(1000,
                layer->GetAnimator()->GetTransitionDuration().InMilliseconds());
    }
    window->Show();
    layer->GetAnimator()->StopAnimating();
  }

  // Test that duration respects default.
  {
    layer = window->layer();
    // Query default duration.
    MinimizeAnimationObserver observer(layer->GetAnimator());
    WindowState::Get(window.get())->Minimize();
    EXPECT_TRUE(layer->GetAnimator()->is_animating());
    base::TimeDelta default_duration(observer.duration());
    window->Show();
    layer->GetAnimator()->StopAnimating();

    layer = window->layer();
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.LockTransitionDuration();
    // Setting transition duration is ignored since duration is locked
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(1000));
    WindowState::Get(window.get())->Minimize();
    EXPECT_TRUE(layer->GetAnimator()->is_animating());
    // Expect default duration (200ms for stock ash minimizing animation).
    EXPECT_EQ(default_duration.InMilliseconds(),
              layer->GetAnimator()->GetTransitionDuration().InMilliseconds());
    window->Show();
    layer->GetAnimator()->StopAnimating();
  }
}

// Test that a slide out animation slides the window off the screen while
// modifying the opacity.
TEST_F(WindowAnimationsTest, SlideOutAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  EXPECT_TRUE(window->layer()->visible());

  ::wm::SetWindowVisibilityAnimationType(
      window.get(), WINDOW_VISIBILITY_ANIMATION_TYPE_FADE_IN_SLIDE_OUT);
  AnimateOnChildWindowVisibilityChanged(window.get(), false);

  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_FALSE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(-150, 0, 100, 100), window->layer()->GetTargetBounds());
}

// Test that a fade in slide out animation fades in.
TEST_F(WindowAnimationsTest, FadeInAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Hide();
  EXPECT_FALSE(window->layer()->visible());

  ::wm::SetWindowVisibilityAnimationType(
      window.get(), WINDOW_VISIBILITY_ANIMATION_TYPE_FADE_IN_SLIDE_OUT);
  AnimateOnChildWindowVisibilityChanged(window.get(), true);

  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_TRUE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), window->layer()->GetTargetBounds());
}

TEST_F(WindowAnimationsTest, SlideOutAnimationPlaysTwiceForPipWindow) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  window->SetBounds(gfx::Rect(8, 8, 100, 100));

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());

  window->Show();
  EXPECT_TRUE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(8, 8, 100, 100), window->layer()->GetTargetBounds());

  window->Hide();
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_FALSE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(-142, 8, 100, 100), window->layer()->GetTargetBounds());

  // Reset the position and try again.
  window->Show();
  window->SetBounds(gfx::Rect(8, 8, 100, 100));
  EXPECT_TRUE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(8, 8, 100, 100), window->layer()->GetTargetBounds());

  window->Hide();
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_FALSE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(-142, 8, 100, 100), window->layer()->GetTargetBounds());
}

TEST_F(WindowAnimationsTest, ResetAnimationAfterDismissingArcPip) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  window->SetBounds(gfx::Rect(8, 8, 100, 100));

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());

  window->Show();
  EXPECT_TRUE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(8, 8, 100, 100), window->layer()->GetTargetBounds());

  // Ensure the window is slided out.
  WindowState::Get(window.get())->Minimize();
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_FALSE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(-142, 8, 100, 100), window->layer()->GetTargetBounds());

  WindowState::Get(window.get())->Maximize();
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_TRUE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600 - ShelfConfig::Get()->shelf_size()),
            window->layer()->GetTargetBounds());

  // Ensure the window is not slided out.
  window->Hide();
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_FALSE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600 - ShelfConfig::Get()->shelf_size()),
            window->layer()->GetTargetBounds());
}

}  // namespace ash
