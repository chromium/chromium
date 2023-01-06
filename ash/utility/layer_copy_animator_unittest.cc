// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/layer_copy_animator.h"

#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/test_compositor_host.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/compositor/test/test_layer_animation_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

class TestLayerCopyAnimator final : public LayerCopyAnimator {
 public:
  explicit TestLayerCopyAnimator(aura::Window* window)
      : LayerCopyAnimator(window) {}
  TestLayerCopyAnimator(const TestLayerCopyAnimator& animator) = delete;
  TestLayerCopyAnimator& operator=(const TestLayerCopyAnimator& animator) =
      delete;
  ~TestLayerCopyAnimator() final = default;

  // LayerCopyAnimator:
  void OnLayerCopied(std::unique_ptr<ui::Layer> new_layer) override {
    DCHECK(!copied_);
    LayerCopyAnimator::OnLayerCopied(std::move(new_layer));
    copied_ = true;
    if (run_loop_.running())
      run_loop_.Quit();
  }

  ui::Layer* WaitForCopy() {
    if (!copied_)
      run_loop_.Run();
    return copied_layer_for_test();
  }

 private:
  base::RunLoop run_loop_;
  bool copied_ = false;
};

class LayerCopyAnimatorTest : public testing::Test {
 public:
  LayerCopyAnimatorTest() = default;
  LayerCopyAnimatorTest(const LayerCopyAnimatorTest&) = delete;
  LayerCopyAnimatorTest& operator=(const LayerCopyAnimatorTest&) = delete;
  ~LayerCopyAnimatorTest() override = default;

  // testing::Test:
  void SetUp() override {
    context_factories_ = std::make_unique<ui::TestContextFactories>(false);

    const gfx::Rect bounds(300, 300);
    host_.reset(ui::TestCompositorHost::Create(
        bounds, context_factories_->GetContextFactory()));
    host_->Show();

    root_.Init(ui::LAYER_NOT_DRAWN);
    root_.SetBounds(gfx::Rect(200, 200));
    compositor()->SetRootLayer(root_.layer());

    anim_root_ = std::make_unique<aura::Window>(nullptr);
    anim_root_->Init(ui::LAYER_NOT_DRAWN);
    anim_root_->SetBounds(gfx::Rect(100, 100));

    anim_leaf_.Init(ui::LAYER_TEXTURED);
    anim_leaf_.SetBounds(gfx::Rect(50, 50));

    root_.AddChild(anim_root_.get());
    anim_root_->AddChild(&anim_leaf_);
  }

  void TearDown() override {
    DeleteAnimRoot();
    host_.reset();
    context_factories_.reset();
  }

  void Advance(const base::TimeDelta& delta) {
    task_environment_.FastForwardBy(delta);
  }

  void GenerateOneFrame() { compositor()->ScheduleFullRedraw(); }

  ui::Compositor* compositor() { return host_->GetCompositor(); }

  aura::Window* root() { return &root_; }
  aura::Window* anim_root() { return anim_root_.get(); }

  void DeleteAnimRoot() {
    if (anim_root_) {
      anim_root_->RemoveChild(&anim_leaf_);
      anim_root_.reset();
    }
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI};

  std::unique_ptr<ui::TestContextFactories> context_factories_;
  std::unique_ptr<ui::TestCompositorHost> host_;
  aura::Window root_{nullptr};

  std::unique_ptr<aura::Window> anim_root_;
  aura::Window anim_leaf_{nullptr};
};

}  // namespace

TEST_F(LayerCopyAnimatorTest, Basic) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* root_layer = root()->layer();
  auto* anim_layer = anim_root()->layer();

  auto* animator = new TestLayerCopyAnimator(anim_root());
  EXPECT_FALSE(animator->animation_requested());
  EXPECT_EQ(2u, anim_layer->children().size());
  EXPECT_EQ(1u, root_layer->children().size());

  GenerateOneFrame();
  auto* copied_layer = animator->WaitForCopy();
  EXPECT_TRUE(copied_layer);

  EXPECT_EQ(ui::LAYER_SOLID_COLOR, copied_layer->type());
  EXPECT_EQ(gfx::Size(100, 100), copied_layer->size());

  ui::TestLayerAnimationObserver observer;
  animator->MaybeStartAnimation(
      &observer, base::BindOnce([](ui::Layer* layer,
                                   ui::LayerAnimationObserver* observer) {
        DCHECK(observer);
        layer->SetOpacity(0.f);
        ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
        settings.SetTransitionDuration(base::Milliseconds(1));
        layer->SetOpacity(1.f);

        ui::LayerAnimationSequence* sequence = new ui::LayerAnimationSequence(
            ui::LayerAnimationElement::CreateOpacityElement(1.0,
                                                            base::TimeDelta()));
        sequence->AddObserver(observer);
        layer->GetAnimator()->ScheduleAnimation(sequence);
      }));
  ASSERT_EQ(2u, root_layer->children().size());
  EXPECT_EQ(copied_layer, root_layer->children()[1]);
  EXPECT_TRUE(copied_layer->GetAnimator()->is_animating());
  EXPECT_EQ(0.f, anim_layer->GetTargetOpacity());

  Advance(base::Milliseconds(1000));
  EXPECT_EQ(3, observer.last_ended_sequence_epoch());
  EXPECT_EQ(1u, root_layer->children().size());
  EXPECT_EQ(1.f, anim_layer->GetTargetOpacity());
}

TEST_F(LayerCopyAnimatorTest, CopyAfterAnimationRequest) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* root_layer = root()->layer();
  auto* anim_layer = anim_root()->layer();

  auto* animator = new TestLayerCopyAnimator(anim_root());
  EXPECT_FALSE(animator->animation_requested());
  EXPECT_EQ(2u, anim_layer->children().size());
  EXPECT_EQ(1u, root_layer->children().size());

  ui::TestLayerAnimationObserver observer;

  animator->MaybeStartAnimation(
      &observer, base::BindOnce([](ui::Layer* layer,
                                   ui::LayerAnimationObserver* observer) {
        DCHECK(observer);
        layer->SetOpacity(0.f);
        ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
        // Longer duration so that animation doesn't end after copy.
        settings.SetTransitionDuration(base::Milliseconds(100));
        layer->SetOpacity(1.f);

        ui::LayerAnimationSequence* sequence = new ui::LayerAnimationSequence(
            ui::LayerAnimationElement::CreateOpacityElement(1.0,
                                                            base::TimeDelta()));
        sequence->AddObserver(observer);
        layer->GetAnimator()->ScheduleAnimation(sequence);
      }));

  EXPECT_FALSE(animator->copied_layer_for_test());

  GenerateOneFrame();
  auto* copied_layer = animator->WaitForCopy();
  ASSERT_TRUE(copied_layer);

  EXPECT_EQ(ui::LAYER_SOLID_COLOR, copied_layer->type());
  EXPECT_EQ(gfx::Size(100, 100), copied_layer->size());
  ASSERT_EQ(2u, root_layer->children().size());
  EXPECT_EQ(copied_layer, root_layer->children()[1]);
  EXPECT_TRUE(copied_layer->GetAnimator()->is_animating());
  EXPECT_EQ(0.f, anim_layer->GetTargetOpacity());

  Advance(base::Milliseconds(1000));

  // When animation starts before copy, it registers the observer to fake
  // sequecne, hence become 6.
  EXPECT_EQ(6, observer.last_ended_sequence_epoch());
  EXPECT_EQ(1u, root_layer->children().size());
  EXPECT_EQ(1.f, anim_layer->GetTargetOpacity());
}

TEST_F(LayerCopyAnimatorTest, CancelByResize) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* root_layer = root()->layer();
  auto* anim_layer = anim_root()->layer();

  auto* animator = new TestLayerCopyAnimator(anim_root());
  EXPECT_FALSE(animator->animation_requested());
  EXPECT_EQ(2u, anim_layer->children().size());
  EXPECT_EQ(1u, root_layer->children().size());

  anim_layer->SetBounds(gfx::Rect(210, 210));
  GenerateOneFrame();
  auto* copied_layer = animator->WaitForCopy();
  ASSERT_FALSE(copied_layer);

  ui::TestLayerAnimationObserver observer;
  EXPECT_EQ(-1, observer.last_aborted_sequence_epoch());
  animator->MaybeStartAnimation(
      &observer, base::BindOnce([](ui::Layer* layer,
                                   ui::LayerAnimationObserver* observer) {
        EXPECT_FALSE(true) << "Callback should not be called";
      }));
  EXPECT_EQ(1.f, anim_layer->GetTargetOpacity());
  EXPECT_EQ(1, observer.last_aborted_sequence_epoch());
}

TEST_F(LayerCopyAnimatorTest, CancelByDelete) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* root_layer = root()->layer();
  auto* anim_layer = anim_root()->layer();

  auto* animator = new LayerCopyAnimator(anim_root());
  EXPECT_FALSE(animator->animation_requested());
  EXPECT_EQ(2u, anim_layer->children().size());
  EXPECT_EQ(1u, root_layer->children().size());

  GenerateOneFrame();
  DeleteAnimRoot();
}

TEST_F(LayerCopyAnimatorTest, CancelByStop) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* root_layer = root()->layer();
  auto* anim_layer = anim_root()->layer();

  auto* animator = new TestLayerCopyAnimator(anim_root());
  EXPECT_FALSE(animator->animation_requested());
  EXPECT_EQ(2u, anim_layer->children().size());
  EXPECT_EQ(1u, root_layer->children().size());

  GenerateOneFrame();
  auto* copied_layer = animator->WaitForCopy();
  ASSERT_TRUE(copied_layer);

  EXPECT_EQ(ui::LAYER_SOLID_COLOR, copied_layer->type());
  EXPECT_EQ(gfx::Size(100, 100), copied_layer->size());

  ui::TestLayerAnimationObserver observer;

  animator->MaybeStartAnimation(
      &observer, base::BindOnce([](ui::Layer* layer,
                                   ui::LayerAnimationObserver* observer) {
        DCHECK(observer);
        layer->SetOpacity(0.f);
        ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
        settings.SetTransitionDuration(base::Milliseconds(100));
        layer->SetOpacity(1.f);

        ui::LayerAnimationSequence* sequence = new ui::LayerAnimationSequence(
            ui::LayerAnimationElement::CreateOpacityElement(1.0,
                                                            base::TimeDelta()));
        sequence->AddObserver(observer);
        layer->GetAnimator()->ScheduleAnimation(sequence);
      }));
  ASSERT_EQ(2u, root_layer->children().size());
  EXPECT_EQ(copied_layer, root_layer->children()[1]);
  EXPECT_TRUE(copied_layer->GetAnimator()->is_animating());
  EXPECT_EQ(0.f, anim_layer->GetTargetOpacity());
  copied_layer->GetAnimator()->StopAnimating();

  Advance(base::Milliseconds(1000));

  EXPECT_EQ(3, observer.last_ended_sequence_epoch());
  EXPECT_EQ(1u, root_layer->children().size());
  EXPECT_EQ(1.f, anim_layer->GetTargetOpacity());
}

TEST_F(LayerCopyAnimatorTest, NoAnimationStopImmediately) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* root_layer = root()->layer();
  auto* anim_layer = anim_root()->layer();

  auto* animator = new TestLayerCopyAnimator(anim_root());
  EXPECT_FALSE(animator->animation_requested());
  EXPECT_EQ(2u, anim_layer->children().size());
  EXPECT_EQ(1u, root_layer->children().size());

  GenerateOneFrame();
  auto* copied_layer = animator->WaitForCopy();
  ASSERT_TRUE(copied_layer);

  EXPECT_EQ(ui::LAYER_SOLID_COLOR, copied_layer->type());
  EXPECT_EQ(gfx::Size(100, 100), copied_layer->size());

  ui::TestLayerAnimationObserver observer;

  animator->MaybeStartAnimation(
      &observer, base::BindOnce([](ui::Layer* layer,
                                   ui::LayerAnimationObserver* observer) {}));
  EXPECT_EQ(1u, root_layer->children().size());
  EXPECT_EQ(1.f, anim_layer->GetTargetOpacity());
  EXPECT_EQ(1, observer.last_ended_sequence_epoch());
}

}  //  namespace ash
