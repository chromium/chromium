// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_widget_fade_animator.h"

#include "base/test/task_environment.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/widget_fade_animator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

class PictureInPictureWidgetFadeAnimatorTest : public views::ViewsTestBase {
 public:
  PictureInPictureWidgetFadeAnimatorTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  PictureInPictureWidgetFadeAnimatorTest(
      const PictureInPictureWidgetFadeAnimatorTest&) = delete;
  PictureInPictureWidgetFadeAnimatorTest& operator=(
      const PictureInPictureWidgetFadeAnimatorTest&) = delete;

  void SetUp() override {
    ViewsTestBase::SetUp();
    fade_animator_ = std::make_unique<PictureInPictureWidgetFadeAnimator>();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  }

  void TearDown() override {
    if (widget_) {
      widget_.reset();
    }
    ViewsTestBase::TearDown();
  }

  bool IsFadingIn() {
    return fade_animator()->GetWidgetFadeAnimatorForTesting()->IsFadingIn();
  }

  views::Widget* widget() { return widget_.get(); }

  PictureInPictureWidgetFadeAnimator* fade_animator() {
    return fade_animator_.get();
  }

 protected:
  void FastForwardPastAnimationDuration() {
    task_environment()->FastForwardBy(base::Milliseconds(
        PictureInPictureWidgetFadeAnimator::kFadeInDurationMs * 2));
  }
  void FastForwardToMiddleOfAnimation() {
    task_environment()->FastForwardBy(base::Milliseconds(
        PictureInPictureWidgetFadeAnimator::kFadeInDurationMs / 2));
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<PictureInPictureWidgetFadeAnimator> fade_animator_;
};

}  // anonymous namespace

TEST_F(PictureInPictureWidgetFadeAnimatorTest,
       AnimateShowWindow_CreatesAndStartsAnimation) {
  ASSERT_NE(nullptr, widget());
  ASSERT_FALSE(widget()->IsVisible());
  ASSERT_NE(nullptr, widget()->GetLayer());
  ASSERT_FLOAT_EQ(1.0f, widget()->GetLayer()->opacity());

  // Start animation.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowInactive);

  // Fast-forward to the middle of the animation and, verify that the fade
  // animator is fading in.
  FastForwardToMiddleOfAnimation();
  EXPECT_TRUE(widget()->IsVisible());
  EXPECT_TRUE(IsFadingIn());

  // Fast-forward past the end of the animation and, verify that the fade
  // animator is not fading in.
  FastForwardPastAnimationDuration();
  EXPECT_TRUE(widget()->IsVisible());
  EXPECT_FALSE(IsFadingIn());
}

TEST_F(PictureInPictureWidgetFadeAnimatorTest,
       CancelAndReset_WhenAnimatorExists_ResetsAnimator) {
  // Start animation.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowInactive);

  // Fast-forward to the middle of the animation and, verify that the fade
  // animator is fading in.
  FastForwardToMiddleOfAnimation();
  EXPECT_TRUE(widget()->IsVisible());
  EXPECT_TRUE(IsFadingIn());

  // Cancel and reset animator in the middle of the animation and, verify that
  // the widget remains visible and the fade animator is reset.
  fade_animator()->CancelAndReset();
  EXPECT_TRUE(widget()->IsVisible());
  EXPECT_EQ(nullptr, fade_animator()->GetWidgetFadeAnimatorForTesting());
}

TEST_F(PictureInPictureWidgetFadeAnimatorTest,
       CancelAndReset_WhenNotAnimating_DoesNothing) {
  ASSERT_NE(nullptr, widget());
  ASSERT_FALSE(widget()->IsVisible());
  ASSERT_NE(nullptr, widget()->GetLayer());
  ASSERT_FLOAT_EQ(1.0f, widget()->GetLayer()->opacity());

  // Calling `CancelAndReset` when the animation has not started should not
  // crash.
  fade_animator()->CancelAndReset();
  ASSERT_FALSE(widget()->IsVisible());
  EXPECT_EQ(nullptr, fade_animator()->GetWidgetFadeAnimatorForTesting());
}

TEST_F(PictureInPictureWidgetFadeAnimatorTest,
       AnimateShowWindow_OnVisibleWidget_DestroysAnimator) {
  // Start animation.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowInactive);

  // Fast-forward to the middle of the animation. The widget is now visible.
  FastForwardToMiddleOfAnimation();
  EXPECT_TRUE(IsFadingIn());

  // A second call to `AnimateShowWindow` on a visible widget should cancel the
  // animation and destroy the animator, but not start a new one.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowActive);
  EXPECT_TRUE(widget()->IsVisible());
  EXPECT_EQ(nullptr, fade_animator()->GetWidgetFadeAnimatorForTesting());
  EXPECT_FLOAT_EQ(1.0f, widget()->GetLayer()->opacity());
}

TEST_F(PictureInPictureWidgetFadeAnimatorTest,
       AnimateShowWindow_MultipleCalls_RecreatesAnimatorIfNotVisible) {
  // Start animation.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowInactive);

  // Fast-forward to the middle of the animation. The widget is now visible.
  FastForwardToMiddleOfAnimation();
  EXPECT_TRUE(IsFadingIn());

  // A second call to `AnimateShowWindow` on a visible widget should cancel the
  // animation and destroy the animator, but not start a new one.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowActive);
  EXPECT_TRUE(widget()->IsVisible());
  EXPECT_EQ(nullptr, fade_animator()->GetWidgetFadeAnimatorForTesting());
  EXPECT_FLOAT_EQ(1.0f, widget()->GetLayer()->opacity());

  // Hide the widget.
  widget()->Hide();
  ASSERT_FALSE(widget()->IsVisible());

  // Calling `AnimateShowWindow` after hiding the widget should start a new
  // animation.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowActive);
  EXPECT_TRUE(widget()->IsVisible());
  EXPECT_TRUE(IsFadingIn());
  EXPECT_NE(nullptr, fade_animator()->GetWidgetFadeAnimatorForTesting());
}

TEST_F(PictureInPictureWidgetFadeAnimatorTest,
       Destructor_WithActiveAnimation_CleansUp) {
  // Create a local fade animator and start the animation.
  std::unique_ptr<PictureInPictureWidgetFadeAnimator> local_fade_animator =
      std::make_unique<PictureInPictureWidgetFadeAnimator>();
  local_fade_animator->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowInactive);

  // Fast-forward to the middle of the animation and, verify that the fade
  // animator is fading in.
  FastForwardToMiddleOfAnimation();
  EXPECT_TRUE(
      local_fade_animator->GetWidgetFadeAnimatorForTesting()->IsFadingIn());

  // Resetting the local animator should not crash and the widget should remain
  // visible.
  local_fade_animator.reset();
  EXPECT_TRUE(widget()->IsVisible());
}

TEST_F(PictureInPictureWidgetFadeAnimatorTest,
       CancelAndReset_DoesNotResetFadeInCallsCount) {
  EXPECT_EQ(0, fade_animator()->GetFadeInCallsCountForTesting());

  // Start animation and verify the fade in calls count.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowInactive);
  EXPECT_EQ(1, fade_animator()->GetFadeInCallsCountForTesting());

  // Verify that calling `CancelAndReset` does not set the fade in calls count
  // to 0.
  fade_animator()->CancelAndReset();
  EXPECT_EQ(1, fade_animator()->GetFadeInCallsCountForTesting());
}

TEST_F(PictureInPictureWidgetFadeAnimatorTest,
       AnimateShowWindow_MultipleCalls_IncrementsFadeInCallsCountIfNotVisible) {
  EXPECT_EQ(0, fade_animator()->GetFadeInCallsCountForTesting());

  // Start animation and verify the fade in calls count.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowInactive);
  EXPECT_EQ(1, fade_animator()->GetFadeInCallsCountForTesting());

  // Hide the widget so we can test that the count increments again.
  widget()->Hide();
  ASSERT_FALSE(widget()->IsVisible());

  // Second call to `AnimateShowWindow` should increase the fade in calls count.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowActive);
  EXPECT_EQ(2, fade_animator()->GetFadeInCallsCountForTesting());
}

TEST_F(
    PictureInPictureWidgetFadeAnimatorTest,
    AnimateShowWindow_MultipleCalls_DoesNotIncrementFadeInCallsCountIfVisible) {
  EXPECT_EQ(0, fade_animator()->GetFadeInCallsCountForTesting());

  // Start animation and verify the fade in calls count.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowInactive);
  EXPECT_EQ(1, fade_animator()->GetFadeInCallsCountForTesting());

  // Make sure the widget is visible before the second call.
  FastForwardToMiddleOfAnimation();
  ASSERT_TRUE(widget()->IsVisible());

  // Second call to `AnimateShowWindow` should not increase the fade in calls
  // count because the widget is already visible.
  fade_animator()->AnimateShowWindow(
      widget(),
      PictureInPictureWidgetFadeAnimator::WidgetShowType::kShowActive);
  EXPECT_EQ(1, fade_animator()->GetFadeInCallsCountForTesting());
}
