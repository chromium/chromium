// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include <tuple>

#include "ash/constants/ash_features.h"
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
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

using aura::Window;
using ui::Layer;

namespace ash {

namespace {

void WaitForMilliseconds(int ms) {
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Milliseconds(ms));
  loop.Run();
}

// TODO(crbug.com/252543343): Consolidate LayerDestroyedChecker into a shared
// test utility.
class LayerDestroyedChecker : public ui::LayerObserver {
 public:
  explicit LayerDestroyedChecker(ui::Layer* layer) { layer->AddObserver(this); }
  LayerDestroyedChecker(const LayerDestroyedChecker&) = delete;
  LayerDestroyedChecker& operator=(const LayerDestroyedChecker&) = delete;
  ~LayerDestroyedChecker() override = default;

  void LayerDestroyed(ui::Layer* layer) override {
    layer->RemoveObserver(this);
    destroyed_ = true;
  }
  bool destroyed() const { return destroyed_; }

 private:
  bool destroyed_ = false;
};

}  // namespace

class WindowAnimationsTest : public AshTestBase {
 public:
  WindowAnimationsTest() = default;

  WindowAnimationsTest(const WindowAnimationsTest&) = delete;
  WindowAnimationsTest& operator=(const WindowAnimationsTest&) = delete;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
  }
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

  MinimizeAnimationObserver(const MinimizeAnimationObserver&) = delete;
  MinimizeAnimationObserver& operator=(const MinimizeAnimationObserver&) =
      delete;

  ~MinimizeAnimationObserver() override { animator_ = nullptr; }

  base::TimeDelta duration() { return duration_; }

 protected:
  // ui::LayerAnimationObserver:
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {
    duration_ = animator_->GetTransitionDuration();
    animator_->RemoveObserver(this);
    animator_ = nullptr;
  }
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}

 private:
  raw_ptr<ui::LayerAnimator> animator_;
  base::TimeDelta duration_;
};

// This is the class that simulates the behavior of
// `FrameHeader::FrameAnimatorView` which may recreate the window layer in the
// middle of setting the animation of the old and new layer.
class FrameAnimator : public ui::ImplicitAnimationObserver {
 public:
  explicit FrameAnimator(aura::Window* window) : window_(window) {
    // Set up an animation which will be stopped before the old layer animation.
    SetOpacityAnimation(window_->layer());
  }

  FrameAnimator(const FrameAnimator&) = delete;
  FrameAnimator& operator=(const FrameAnimator&) = delete;
  ~FrameAnimator() override = default;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    // Once the initial animation is stopped by the old layer, start a new
    // opacity animation and recreate the window layer at the same time. The
    // opacity animation will be stopped when the layer set opacity and the
    // layer is destroyed.
    if (!animation_started_)
      StartAnimation();
    else
      layer_owner_.reset();
  }

 private:
  // Set an opacity animation which should last longer than the cross fade
  // animation.
  void SetOpacityAnimation(ui::Layer* layer) {
    layer->SetOpacity(1.f);
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.AddObserver(this);
    settings.SetTransitionDuration(base::Milliseconds(1000));
    layer->SetOpacity(0.f);
  }

  // Recreate the window layer and start a new opacity animation.
  void StartAnimation() {
    layer_owner_ =
        std::make_unique<ui::LayerTreeOwner>(window_->RecreateLayer());
    SetOpacityAnimation(layer_owner_->root());
    animation_started_ = true;
  }

  raw_ptr<aura::Window> window_;
  std::unique_ptr<ui::LayerTreeOwner> layer_owner_;
  bool animation_started_ = false;
};

TEST_F(WindowAnimationsTest, HideShowBrightnessGrayscaleAnimation) {
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell({.window_id = 0}));
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
                                       base::Seconds(5));
  EXPECT_EQ(0.0f, window->layer()->GetTargetBrightness());
  EXPECT_EQ(0.0f, window->layer()->GetTargetGrayscale());
  EXPECT_TRUE(window->layer()->visible());
}

TEST_F(WindowAnimationsTest, LayerTargetVisibility) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell({.window_id = 0}));

  // Layer target visibility changes according to Show/Hide.
  window->Show();
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  window->Hide();
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  window->Show();
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
}

TEST_F(WindowAnimationsTest, CrossFadeToBounds) {
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  window->SetBounds(gfx::Rect(5, 10, 320, 240));
  window->Show();

  Layer* old_layer = window->layer();
  EXPECT_EQ(1.0f, old_layer->GetTargetOpacity());

  // Cross fade to a larger size, as in a maximize animation.
  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated({640, 480}, kCrossFadeAnimationParams);
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
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());

  // Run the animations to completion.
  old_layer->GetAnimator()->Step(base::TimeTicks::Now() + base::Seconds(1));
  window->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                       base::Seconds(1));

  // Cross fade to a smaller size, as in a restore animation.
  old_layer = window->layer();
  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated({5, 10, 320, 240}, kCrossFadeAnimationParams);
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
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());

  old_layer->GetAnimator()->Step(base::TimeTicks::Now() + base::Seconds(1));
  window->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                       base::Seconds(1));
}

TEST_F(WindowAnimationsTest, CrossFadeInterrupted) {
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  window->SetBounds({5, 10, 320, 240});
  window->Show();
  window->layer()->GetAnimator()->StopAnimating();

  Layer* first_layer = window->layer();
  LayerDestroyedChecker first_layer_checker(first_layer);

  // Start the first cross-fade animation to a larger size.
  std::unique_ptr<ui::LayerTreeOwner> first_old_layer_owner =
      wm::RecreateLayers(window.get());
  window->SetBounds({640, 480});
  CrossFadeAnimation(window.get(), std::move(first_old_layer_owner));
  Layer* second_layer = window->layer();
  LayerDestroyedChecker second_layer_checker(second_layer);
  EXPECT_NE(first_layer, second_layer);
  EXPECT_FALSE(first_layer_checker.destroyed());

  gfx::Transform initial_in_transform;
  initial_in_transform.Translate(5.f, 10.f);
  initial_in_transform.Scale(320.f / 640.f, 240.f / 480.f);
  EXPECT_EQ(initial_in_transform, second_layer->transform());
  EXPECT_TRUE(second_layer->GetTargetTransform().IsIdentity());

  // Step the first cross-fade animation halfway through.
  ui::LayerAnimatorTestController test_controller(second_layer->GetAnimator());
  test_controller.StartThreadedAnimationsIfNeeded(
      second_layer->GetAnimator()->last_step_time());
  const base::TimeDelta duration =
      test_controller.GetRunningSequence(ui::LayerAnimationElement::TRANSFORM)
          ->FirstElement()
          ->duration();
  test_controller.Step(duration / 2);

  const double tween_value =
      gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, 0.5);
  const gfx::Transform interrupted_transform =
      gfx::Tween::TransformValueBetween(tween_value, initial_in_transform,
                                        gfx::Transform());

  // Interrupt with a new cross-fade animation to {10, 20, 400, 300}.
  std::unique_ptr<ui::LayerTreeOwner> second_old_layer_owner =
      wm::RecreateLayers(window.get());
  window->SetBounds({10, 20, 400, 300});
  CrossFadeAnimation(window.get(), std::move(second_old_layer_owner));

  // The old layer from the first animation is deleted, `second_layer` is
  // detached and becomes the old layer, and `third_layer` is recreated.
  EXPECT_TRUE(first_layer_checker.destroyed());
  EXPECT_FALSE(second_layer_checker.destroyed());
  Layer* third_layer = window->layer();
  EXPECT_NE(second_layer, third_layer);

  // The detached `second_layer` (now old layer) starts from the interrupted
  // transform and animates to the transform for the new bounds.
  EXPECT_EQ(interrupted_transform, second_layer->transform());
  gfx::Transform expected_old_target_transform;
  expected_old_target_transform.Translate(10.f, 20.f);
  expected_old_target_transform.Scale(400.f / 640.f, 300.f / 480.f);
  EXPECT_EQ(expected_old_target_transform, second_layer->GetTargetTransform());
  EXPECT_EQ(1.0f, second_layer->opacity());
  EXPECT_EQ(0.0f, second_layer->GetTargetOpacity());

  // The newly recreated `third_layer` has the interpolated transform as its
  // initial value and animates to the identity transform.
  const gfx::RectF interrupted_bounds =
      interrupted_transform.MapRect(gfx::RectF(640.f, 480.f));
  gfx::Transform expected_new_initial_transform;
  expected_new_initial_transform.Translate(interrupted_bounds.x() - 10.f,
                                           interrupted_bounds.y() - 20.f);
  expected_new_initial_transform.Scale(interrupted_bounds.width() / 400.f,
                                       interrupted_bounds.height() / 300.f);
  EXPECT_EQ(expected_new_initial_transform, third_layer->transform());
  EXPECT_TRUE(third_layer->GetTargetTransform().IsIdentity());
  EXPECT_EQ(1.0f, third_layer->GetTargetOpacity());

  third_layer->GetAnimator()->StopAnimating();
  EXPECT_TRUE(second_layer_checker.destroyed());
  EXPECT_TRUE(third_layer->transform().IsIdentity());
}

TEST_F(WindowAnimationsTest, PositionAnimation) {
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  window->SetBounds({10, 20, 400, 300});
  window->Show();
  window->layer()->GetAnimator()->StopAnimating();

  Layer* layer = window->layer();
  gfx::Point old_origin = window->bounds().origin();
  window->SetBounds({30, 50, 400, 300});
  PositionAnimation(window.get(), old_origin);

  EXPECT_EQ(layer, window->layer());
  EXPECT_EQ(gfx::Transform::MakeTranslation(-20.f, -30.f),
            window->layer()->transform());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());

  // Step halfway through (visual origin (20, 35)) and interrupt with a new
  // position animation to {60, 80, 400, 300}.
  {
    ui::LayerAnimatorTestController test_controller(
        window->layer()->GetAnimator());
    test_controller.StartThreadedAnimationsIfNeeded(
        window->layer()->GetAnimator()->last_step_time());
    ui::LayerAnimationSequence* sequence = test_controller.GetRunningSequence(
        ui::LayerAnimationElement::TRANSFORM);
    ASSERT_TRUE(sequence);
    test_controller.Step(sequence->FirstElement()->duration() / 2);
  }
  old_origin = window->bounds().origin();
  window->SetBounds({60, 80, 400, 300});
  PositionAnimation(window.get(), old_origin, kFloatCrossFadeAnimationParams);

  EXPECT_EQ(layer, window->layer());
  EXPECT_EQ(gfx::Transform::MakeTranslation(-40.f, -45.f),
            window->layer()->transform());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  {
    ui::LayerAnimatorTestController test_controller(
        window->layer()->GetAnimator());
    ui::LayerAnimationSequence* sequence = test_controller.GetRunningSequence(
        ui::LayerAnimationElement::TRANSFORM);
    ASSERT_TRUE(sequence);
    EXPECT_EQ(gfx::Tween::ACCEL_30_DECEL_20_85,
              sequence->FirstElement()->tween_type());
    EXPECT_EQ(gfx::ScopedAnimationDurationScaleMode::duration_multiplier() *
                  BoundsChangeAnimationParams::kFloatUnfloatDuration,
              sequence->FirstElement()->duration());
  }

  window->layer()->GetAnimator()->StopAnimating();

  // Calling with unchanged origin does not start a no-op animation.
  PositionAnimation(window.get(), window->bounds().origin());
  EXPECT_FALSE(window->layer()->GetAnimator()->is_animating());

  // Interrupting an ongoing position animation when the new target origin
  // matches the mid-flight visual origin resets the transform to identity
  // without starting a new animation.
  old_origin = window->bounds().origin();
  window->SetBounds({80, 100, 400, 300});
  PositionAnimation(window.get(), old_origin);
  {
    ui::LayerAnimatorTestController test_controller(
        window->layer()->GetAnimator());
    test_controller.StartThreadedAnimationsIfNeeded(
        window->layer()->GetAnimator()->last_step_time());
    ui::LayerAnimationSequence* sequence = test_controller.GetRunningSequence(
        ui::LayerAnimationElement::TRANSFORM);
    ASSERT_TRUE(sequence);
    test_controller.Step(sequence->FirstElement()->duration() / 2);
  }
  old_origin = window->bounds().origin();
  window->SetBounds({70, 90, 400, 300});
  PositionAnimation(window.get(), old_origin);
  EXPECT_FALSE(window->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window->layer()->transform().IsIdentity());
}

TEST_F(WindowAnimationsTest, SetBoundsDirectAnimated) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kCrossFadeBoundsChangeAnimation};
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  window->SetBounds({5, 10, 320, 240});
  window->Show();

  Layer* old_layer = window->layer();
  LayerDestroyedChecker old_layer_checker(old_layer);

  // Animating to a different size uses cross-fade animation (recreates layer).
  WindowState::Get(window.get())->SetBoundsDirectAnimated({400, 300});
  EXPECT_TRUE(HasCrossFadeAnimation(window.get()));
  Layer* second_layer = window->layer();
  LayerDestroyedChecker second_layer_checker(second_layer);
  EXPECT_NE(old_layer, second_layer);
  EXPECT_FALSE(old_layer_checker.destroyed());
  EXPECT_EQ(gfx::Rect(400, 300), window->bounds());
  EXPECT_TRUE(second_layer->GetTargetTransform().IsIdentity());

  // Interrupting an ongoing cross-fade animation with the same target size
  // still uses cross-fade animation and deletes the previous old layer.
  WindowState::Get(window.get())->SetBoundsDirectAnimated({10, 20, 400, 300});
  EXPECT_TRUE(HasCrossFadeAnimation(window.get()));
  EXPECT_TRUE(old_layer_checker.destroyed());
  EXPECT_FALSE(second_layer_checker.destroyed());
  EXPECT_NE(second_layer, window->layer());
  EXPECT_EQ(gfx::Rect(10, 20, 400, 300), window->bounds());

  window->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  EXPECT_TRUE(second_layer_checker.destroyed());

  // Animating only position (same size) when not currently cross-fading uses a
  // transform animation without cross-fading or recreating the layer.
  old_layer = window->layer();
  WindowState::Get(window.get())->SetBoundsDirectAnimated({20, 30, 400, 300});
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(old_layer, window->layer());
  EXPECT_EQ((gfx::Rect{20, 30, 400, 300}), window->bounds());
  EXPECT_EQ(gfx::Transform::MakeTranslation(-10.f, -10.f),
            window->layer()->transform());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());

  window->layer()->GetAnimator()->StopAnimating();

  // Calling with identical bounds when not animating does not start a no-op
  // animation.
  WindowState::Get(window.get())->SetBoundsDirectAnimated({20, 30, 400, 300});
  EXPECT_FALSE(window->layer()->GetAnimator()->is_animating());
}

TEST_F(WindowAnimationsTest, SetBoundsDirectAnimatedPositionInterrupted) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kCrossFadeBoundsChangeAnimation};
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  window->SetBounds({10, 20, 400, 300});
  window->Show();
  window->layer()->GetAnimator()->StopAnimating();

  Layer* old_layer = window->layer();
  WindowState::Get(window.get())->SetBoundsDirectAnimated({20, 30, 400, 300});
  ASSERT_FALSE(HasCrossFadeAnimation(window.get()));

  // Step the transform animation halfway through (visual origin (15, 25)) and
  // interrupt with another position-only animation.
  {
    ui::LayerAnimatorTestController test_controller(
        window->layer()->GetAnimator());
    test_controller.StartThreadedAnimationsIfNeeded(
        window->layer()->GetAnimator()->last_step_time());
    ui::LayerAnimationSequence* sequence = test_controller.GetRunningSequence(
        ui::LayerAnimationElement::TRANSFORM);
    ASSERT_TRUE(sequence);
    const base::TimeDelta duration = sequence->FirstElement()->duration();
    test_controller.Step(duration / 2);
  }
  WindowState::Get(window.get())->SetBoundsDirectAnimated({50, 60, 400, 300});
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(old_layer, window->layer());
  EXPECT_EQ((gfx::Rect{50, 60, 400, 300}), window->bounds());
  EXPECT_EQ(gfx::Transform::MakeTranslation(-35.f, -35.f),
            window->layer()->transform());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());

  // Step halfway through again (visual origin (32.5, 42.5)) and interrupt with
  // a resize animation, which should cross-fade from the interrupted position.
  {
    ui::LayerAnimatorTestController test_controller(
        window->layer()->GetAnimator());
    test_controller.StartThreadedAnimationsIfNeeded(
        window->layer()->GetAnimator()->last_step_time());
    ui::LayerAnimationSequence* sequence = test_controller.GetRunningSequence(
        ui::LayerAnimationElement::TRANSFORM);
    ASSERT_TRUE(sequence);
    const base::TimeDelta duration = sequence->FirstElement()->duration();
    test_controller.Step(duration / 2);
  }
  LayerDestroyedChecker third_layer_checker(old_layer);
  WindowState::Get(window.get())->SetBoundsDirectAnimated({10, 20, 500, 400});
  EXPECT_TRUE(HasCrossFadeAnimation(window.get()));
  EXPECT_NE(old_layer, window->layer());
  EXPECT_FALSE(third_layer_checker.destroyed());
  EXPECT_EQ(gfx::Transform::MakeTranslation(-17.5f, -17.5f),
            old_layer->transform());
  gfx::Transform expected_in_transform;
  expected_in_transform.Translate(22.5f, 22.5f);
  expected_in_transform.Scale(400.f / 500.f, 300.f / 400.f);
  EXPECT_EQ(expected_in_transform, window->layer()->transform());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());

  window->layer()->GetAnimator()->StopAnimating();
  EXPECT_TRUE(third_layer_checker.destroyed());
}

TEST_F(WindowAnimationsTest, SetBoundsDirectAnimatedFallbackParams) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kCrossFadeBoundsChangeAnimation};

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  window->SetBounds({20, 30, 400, 300});
  window->Show();

  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Animating only position (same size) with float params falls back to
  // transform animation while preserving the float duration and tween type.
  Layer* old_layer = window->layer();
  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated({30, 40, 400, 300},
                                kFloatCrossFadeAnimationParams);
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(old_layer, window->layer());
  EXPECT_EQ((gfx::Rect{30, 40, 400, 300}), window->bounds());
  EXPECT_EQ(gfx::Transform::MakeTranslation(-10.f, -10.f),
            window->layer()->transform());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  {
    ui::LayerAnimatorTestController test_controller(
        window->layer()->GetAnimator());
    ui::LayerAnimationSequence* sequence = test_controller.GetRunningSequence(
        ui::LayerAnimationElement::TRANSFORM);
    ASSERT_TRUE(sequence);
    ui::LayerAnimationElement* transform_element = sequence->FirstElement();
    EXPECT_EQ(gfx::Tween::ACCEL_30_DECEL_20_85,
              transform_element->tween_type());
    EXPECT_EQ(gfx::ScopedAnimationDurationScaleMode::duration_multiplier() *
                  BoundsChangeAnimationParams::kFloatUnfloatDuration,
              transform_element->duration());
  }
  window->layer()->GetAnimator()->StopAnimating();

  // Moving a newly shown window while its show animation (opacity + scale
  // transform) is still in flight preserves the opacity animation to 1.0f.
  std::unique_ptr<Window> shown_window(
      CreateTestWindowInShell({.window_id = 1}));
  shown_window->SetBounds({20, 30, 400, 300});
  shown_window->Show();
  ASSERT_TRUE(shown_window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::OPACITY));
  WindowState::Get(shown_window.get())
      ->SetBoundsDirectAnimated({40, 50, 400, 300});
  EXPECT_EQ(1.0f, shown_window->layer()->GetTargetOpacity());
  EXPECT_EQ((gfx::Rect{40, 50, 400, 300}), shown_window->bounds());
  EXPECT_TRUE(shown_window->layer()->GetTargetTransform().IsIdentity());
}

TEST_F(WindowAnimationsTest, SetBoundsDirectAnimatedBoundsParams) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kCrossFadeBoundsChangeAnimation};
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  window->SetBounds({30, 40, 400, 300});
  window->Show();
  window->layer()->GetAnimator()->StopAnimating();

  // Animating only position (same size) with `Type::kBounds` also uses
  // `PositionAnimation` (transform animation rather than `BOUNDS` animation).
  Layer* old_layer = window->layer();
  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated({50, 70, 400, 300}, kBoundsAnimationParams);
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(old_layer, window->layer());
  EXPECT_FALSE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS));
  EXPECT_TRUE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));
  EXPECT_EQ((gfx::Rect{50, 70, 400, 300}), window->bounds());
  EXPECT_EQ(gfx::Transform::MakeTranslation(-20.f, -30.f),
            window->layer()->transform());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  window->layer()->GetAnimator()->StopAnimating();

  // Interrupting an ongoing cross-fade animation with `Type::kBounds` and the
  // same target size stops the cross-fade and uses `PositionAnimation`.
  WindowState::Get(window.get())->SetBoundsDirectAnimated({50, 70, 500, 400});
  ASSERT_TRUE(HasCrossFadeAnimation(window.get()));
  Layer* cross_fade_layer = window->layer();
  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated({80, 110, 500, 400}, kBoundsAnimationParams);
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(cross_fade_layer, window->layer());
  EXPECT_FALSE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS));
  EXPECT_TRUE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));
  EXPECT_EQ((gfx::Rect{80, 110, 500, 400}), window->bounds());
  EXPECT_EQ(gfx::Transform::MakeTranslation(-30.f, -40.f),
            window->layer()->transform());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  window->layer()->GetAnimator()->StopAnimating();

  // `gfx::Tween::ZERO` still uses `BOUNDS` animation so the old bounds are held
  // until the end of the delay.
  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated(
          {100, 120, 500, 400},
          {.type = BoundsChangeAnimationParams::Type::kBounds,
           .tween_type = gfx::Tween::ZERO});
  EXPECT_TRUE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS));
  EXPECT_FALSE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));
  EXPECT_EQ((gfx::Rect{80, 110, 500, 400}), window->bounds());
  EXPECT_EQ((gfx::Rect{100, 120, 500, 400}),
            window->layer()->GetTargetBounds());
  window->layer()->GetAnimator()->StopAnimating();
}

TEST_F(WindowAnimationsTest, SetBoundsDirectAnimatedFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kCrossFadeBoundsChangeAnimation);
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  window->SetBounds({30, 40, 400, 300});
  window->Show();
  window->layer()->GetAnimator()->StopAnimating();

  // When `kCrossFadeBoundsChangeAnimation` is disabled, position-only changes
  // with default params (or `kBoundsAnimationParams`) use `BOUNDS` animation
  // rather than `PositionAnimation`.
  Layer* old_layer = window->layer();
  WindowState::Get(window.get())->SetBoundsDirectAnimated({50, 70, 400, 300});
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(old_layer, window->layer());
  EXPECT_TRUE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS));
  EXPECT_FALSE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));
  EXPECT_EQ((gfx::Rect{30, 40, 400, 300}), window->bounds());
  EXPECT_EQ((gfx::Rect{50, 70, 400, 300}), window->layer()->GetTargetBounds());
  window->layer()->GetAnimator()->StopAnimating();

  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated({80, 110, 400, 300}, kBoundsAnimationParams);
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  EXPECT_TRUE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS));
  EXPECT_FALSE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));
  EXPECT_EQ((gfx::Rect{50, 70, 400, 300}), window->bounds());
  EXPECT_EQ((gfx::Rect{80, 110, 400, 300}), window->layer()->GetTargetBounds());
  window->layer()->GetAnimator()->StopAnimating();

  // Size changes with default params also use `BOUNDS` animation.
  WindowState::Get(window.get())->SetBoundsDirectAnimated({80, 110, 500, 400});
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  EXPECT_TRUE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS));
  EXPECT_EQ((gfx::Rect{80, 110, 500, 400}), window->layer()->GetTargetBounds());
  window->layer()->GetAnimator()->StopAnimating();

  // Explicit `Type::kCrossFade` still uses cross-fade animation even when only
  // position changes.
  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated({100, 120, 500, 400},
                                kCrossFadeAnimationParams);
  EXPECT_TRUE(HasCrossFadeAnimation(window.get()));
  window->layer()->GetAnimator()->StopAnimating();
}

// TODO(crbug.com/576888538): Frequently fails due to frame presentation
// timeout.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_SetBoundsDirectAnimatedHistograms \
  DISABLED_SetBoundsDirectAnimatedHistograms
#else
#define MAYBE_SetBoundsDirectAnimatedHistograms \
  SetBoundsDirectAnimatedHistograms
#endif
TEST_F(WindowAnimationsTest, MAYBE_SetBoundsDirectAnimatedHistograms) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kCrossFadeBoundsChangeAnimation};

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  window->SetBounds({5, 10, 320, 240});
  window->Show();

  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Explicitly specifying `Type::kBounds` uses bounds animation even when
  // resizing and the feature flag is enabled, and records `histogram_name` when
  // specified.
  base::HistogramTester histogram_tester;
  constexpr char kTestBoundsHistogram[] = "Ash.Test.BoundsSmoothness";
  Layer* old_layer = window->layer();
  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated(
          {20, 30, 500, 400},
          {.type = BoundsChangeAnimationParams::Type::kBounds,
           .duration = base::Milliseconds(2000),
           .histogram_name = kTestBoundsHistogram});
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(old_layer, window->layer());
  EXPECT_EQ(gfx::Rect(20, 30, 500, 400), window->layer()->GetTargetBounds());
  ui::LayerAnimationStoppedWaiter().Wait(window->layer());
  ui::Compositor* compositor = window->layer()->GetCompositor();
  compositor->ScheduleFullRedraw();
  std::ignore =
      ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(100));
  histogram_tester.ExpectTotalCount(kTestBoundsHistogram, 1);

  // Using `PositionAnimation` behind the scenes (same size) still intentionally
  // records `histogram_name` for both `kCrossFade` and `kBounds`.
  constexpr char kTestPositionHistogram[] = "Ash.Test.PositionSmoothness";
  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated(
          {30, 40, 500, 400},
          {.type = BoundsChangeAnimationParams::Type::kCrossFade,
           .duration = base::Milliseconds(2000),
           .histogram_name = kTestPositionHistogram});
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  ui::LayerAnimationStoppedWaiter().Wait(window->layer());
  compositor->ScheduleFullRedraw();
  std::ignore =
      ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(100));
  histogram_tester.ExpectTotalCount(kTestPositionHistogram, 1);

  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated(
          {40, 50, 500, 400},
          {.type = BoundsChangeAnimationParams::Type::kBounds,
           .duration = base::Milliseconds(2000),
           .histogram_name = kTestBoundsHistogram});
  EXPECT_FALSE(HasCrossFadeAnimation(window.get()));
  ui::LayerAnimationStoppedWaiter().Wait(window->layer());
  compositor->ScheduleFullRedraw();
  std::ignore =
      ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(100));
  histogram_tester.ExpectTotalCount(kTestBoundsHistogram, 2);
}

TEST_F(WindowAnimationsTest, SetBoundsDirectAnimatedEmptyBounds) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kCrossFadeBoundsChangeAnimation};

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  window->SetBounds({20, 30, 500, 400});
  window->Show();

  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Target bounds with zero width and/or height clamp each zero dimension to 1.
  WindowState::Get(window.get())->SetBoundsDirectAnimated({10, 20, 0, 300});
  EXPECT_TRUE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(gfx::Rect(10, 20, 1, 300), window->bounds());
  EXPECT_EQ(gfx::Vector2dF(500.f, 400.f / 300.f),
            window->layer()->transform().To2dScale());
  window->layer()->GetAnimator()->StopAnimating();

  WindowState::Get(window.get())->SetBoundsDirectAnimated({10, 20, 300, 0});
  EXPECT_TRUE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(gfx::Rect(10, 20, 300, 1), window->bounds());
  EXPECT_EQ(gfx::Vector2dF(1.f / 300.f, 300.f),
            window->layer()->transform().To2dScale());
  window->layer()->GetAnimator()->StopAnimating();

  WindowState::Get(window.get())->SetBoundsDirectAnimated({10, 20, 0, 0});
  EXPECT_TRUE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(gfx::Rect(10, 20, 1, 1), window->bounds());
  EXPECT_EQ(gfx::Vector2dF(300.f, 1.f),
            window->layer()->transform().To2dScale());
  window->layer()->GetAnimator()->StopAnimating();

  // Original bounds with zero width and/or height clamp each zero dimension
  // to 1.
  window->SetBounds({5, 6, 0, 250});
  Layer* old_layer = window->layer();
  WindowState::Get(window.get())->SetBoundsDirectAnimated({10, 20, 100, 200});
  EXPECT_TRUE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(gfx::Rect(5, 6, 1, 250), old_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(100.f, 200.f / 250.f),
            old_layer->GetTargetTransform().To2dScale());
  window->layer()->GetAnimator()->StopAnimating();

  window->SetBounds({5, 6, 250, 0});
  old_layer = window->layer();
  WindowState::Get(window.get())->SetBoundsDirectAnimated({10, 20, 100, 200});
  EXPECT_TRUE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(gfx::Rect(5, 6, 250, 1), old_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(100.f / 250.f, 200.f),
            old_layer->GetTargetTransform().To2dScale());
  window->layer()->GetAnimator()->StopAnimating();

  window->SetBounds({});
  old_layer = window->layer();
  WindowState::Get(window.get())->SetBoundsDirectAnimated({10, 20, 100, 200});
  EXPECT_TRUE(HasCrossFadeAnimation(window.get()));
  EXPECT_EQ(gfx::Rect(1, 1), old_layer->bounds());
  EXPECT_EQ(gfx::Vector2dF(100.f, 200.f),
            old_layer->GetTargetTransform().To2dScale());
  window->layer()->GetAnimator()->StopAnimating();
}

// Tests that when crossfading from a window which has a transform, the cross
// fading animation should be ignored and the window should set to its desired
// bounds directly.
TEST_F(WindowAnimationsTest, CrossFadeToBoundsFromTransform) {
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
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
      ->SetBoundsDirectAnimated({640, 480}, kCrossFadeAnimationParams);
  // Window's layer has not been replaced.
  EXPECT_EQ(old_layer, window->layer());
  // Original layer stays opaque and set to new size directly.
  EXPECT_EQ(1.0f, old_layer->GetTargetOpacity());
  EXPECT_EQ("0,0 640x480", old_layer->bounds().ToString());
  // Window still has its old transform before crossfading animation.
  EXPECT_EQ(half_size, old_layer->transform());
}

// Tests that if we recreate the window layers during a cross fade animation,
// there is no crash.
// Regression test for https://crbug.com/1088169.
TEST_F(WindowAnimationsTest, CrossFadeThenRecreate) {
  auto window = CreateWindowWithAppType(chromeos::AppType::NON_APP, {100, 100});

  // Use a bit more time than NON_ZERO_DURATION as its possible with non zero we
  // finish the animation instantly.
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  WindowState* window_state = WindowState::Get(window.get());
  window_state->Maximize();
  ASSERT_TRUE(window->layer()->GetAnimator()->is_animating());

  // Recreate the layers and then delete |window|. There should be no crash when
  // stopping the old layers animation.
  std::unique_ptr<ui::LayerTreeOwner> tree = wm::RecreateLayers(window.get());
  window.reset();
  tree->root()->GetAnimator()->StopAnimating();
}

namespace {

// Defines an observer that would recreate the window's layer tree when the
// opacity is set for the first time on it since the start of the observation.
class WindowOpacityObserver : public aura::WindowObserver {
 public:
  explicit WindowOpacityObserver(aura::Window* window) {
    observation_.Observe(window);
  }
  WindowOpacityObserver(const WindowOpacityObserver&) = delete;
  WindowOpacityObserver& operator=(const WindowOpacityObserver&) = delete;
  ~WindowOpacityObserver() override = default;

  // aura::WindowObserver:
  void OnWindowOpacitySet(aura::Window* window,
                          ui::PropertyChangeReason reason) override {
    // In a cross-fade animation for maximizing, the window's opacity is set to
    // 0 first, at which point we recreate the layers, and then it's set to
    // animate to 1, at which point we destroy the old layer tree to simulate
    // the crash in http://b/333095196.
    if (owner_) {
      owner_.reset();
    } else {
      owner_ = wm::RecreateLayers(window);
    }
  }

 private:
  base::ScopedObservation<aura::Window, aura::WindowObserver> observation_{
      this};
  std::unique_ptr<ui::LayerTreeOwner> owner_;
};

}  // namespace

// Regression test for http://b/333095196 where the window's layer tree is
// recreated while in the middle of a cross fade animation.
TEST_F(WindowAnimationsTest, RecreateLayersDuringCrossFade) {
  auto window = CreateWindowWithAppType(chromeos::AppType::NON_APP, {100, 100});

  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  WindowState* window_state = WindowState::Get(window.get());
  WindowOpacityObserver observer{window.get()};
  window_state->Maximize();
}

// Tests that if the window layer is recreated after setting the old layer's
// animation (e.g., by `FrameHeader::FrameAnimatorView::StartAnimation`). There
// should be no crash. Regression test for https://crbug.com/1313977.
TEST_F(WindowAnimationsTest, RecreateWhenSettingCrossFade) {
  auto window = CreateWindowWithAppType(chromeos::AppType::NON_APP, {100, 100});
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  auto frame_animator = std::make_unique<FrameAnimator>(window.get());
  WindowState::Get(window.get())->Maximize();
}

TEST_F(WindowAnimationsTest, LockAnimationDuration) {
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  Layer* layer = window->layer();
  window->SetBounds(gfx::Rect(5, 10, 320, 240));
  window->Show();

  // Test that it is possible to override transition duration when it is not
  // locked.
  {
    ui::ScopedLayerAnimationSettings settings1(layer->GetAnimator());
    settings1.SetTransitionDuration(base::Milliseconds(1000));
    {
      ui::ScopedLayerAnimationSettings settings2(layer->GetAnimator());
      // Duration is not locked so it gets overridden.
      settings2.SetTransitionDuration(base::Milliseconds(50));
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
    settings1.SetTransitionDuration(base::Milliseconds(1000));
    // Duration is locked in outer scope.
    settings1.LockTransitionDuration();
    {
      ui::ScopedLayerAnimationSettings settings2(layer->GetAnimator());
      // Transition duration setting is ignored.
      settings2.SetTransitionDuration(base::Milliseconds(50));
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
    settings.SetTransitionDuration(base::Milliseconds(1000));
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
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell({.window_id = 0}));
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
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell({.window_id = 0}));
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
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell({.window_id = 0}));
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
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShell({.window_id = 0}));
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

// Tests a version of the cross fade animation which animates the transform and
// opacity of the new layer, but only the opacity of the old layer. The old
// layer transform is updated manually when the animation ticks so that it
// has the same visible bounds as the new layer.
TEST_F(WindowAnimationsTest, CrossFadeAnimateNewLayerOnly) {
  gfx::ScopedAnimationDurationScaleMode test_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowInShell({.window_id = 0}));
  window->SetBounds(gfx::Rect(10, 10, 200, 200));
  window->Show();
  window->layer()->GetAnimator()->StopAnimating();

  Layer* old_layer = window->layer();
  EXPECT_EQ(1.f, old_layer->GetTargetOpacity());

  const gfx::Rect target_bounds(40, 40, 400, 400);
  WindowState::Get(window.get())
      ->SetBoundsDirectAnimated(
          target_bounds,
          {.type =
               BoundsChangeAnimationParams::Type::kCrossFadeAnimateNewLayerOnly,
           .duration = base::Milliseconds(200),
           .tween_type = gfx::Tween::LINEAR,
           .histogram_name = "test-histogram-name"});

  // Window's layer has been replaced.
  EXPECT_NE(old_layer, window->layer());

  // Original layer fades away. Transform is updated as the animation steps.
  EXPECT_EQ(0.f, old_layer->GetTargetOpacity());
  EXPECT_EQ(gfx::Rect(10, 10, 200, 200), old_layer->bounds());
  EXPECT_TRUE(old_layer->GetTargetTransform().IsIdentity());

  // New layer animates in to the identity transform.
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());

  // Start the animations, then set the bounds of the new window during the
  // animation.
  WaitForMilliseconds(10);

  // Set the bounds halfway through the animation. The bounds of the old layer
  // remain the same, but the transform has updated to match the bounds of the
  // new layer.
  window->SetBounds(gfx::Rect(80, 80, 200, 200));
  WaitForMilliseconds(100);
  EXPECT_EQ(gfx::Rect(10, 10, 200, 200), old_layer->bounds());
  EXPECT_FALSE(old_layer->GetTargetTransform().IsIdentity());

  // New layer targets remain the same.
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());

  ui::LayerAnimationStoppedWaiter().Wait(window->layer());
  EXPECT_FALSE(window->layer()->GetAnimator()->is_animating());
}

// Tests that widgets that are created minimized have the correct restore
// bounds.
TEST_F(WindowAnimationsTest, NoMinimizedShowAnimation) {
  gfx::ScopedAnimationDurationScaleMode animation_scale_mode(
      gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  views::UniqueWidgetPtr widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.show_state = ui::mojom::WindowShowState::kMinimized;
  params.bounds = gfx::Rect(600, 400);

  widget->Init(std::move(params));
  auto* layer = widget->GetNativeWindow()->layer();
  widget->Show();
  // The window should have the same layer because layer animation will recreate
  // layer.
  EXPECT_EQ(layer, widget->GetNativeWindow()->layer());
}

}  // namespace ash
