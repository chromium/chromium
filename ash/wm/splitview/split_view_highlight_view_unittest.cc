// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_highlight_view.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/test/icu_test_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class SplitViewHighlightViewTest : public AshTestBase {
 public:
  SplitViewHighlightViewTest() = default;
  ~SplitViewHighlightViewTest() override = default;

  SplitViewHighlightViewTest(const SplitViewHighlightViewTest&) = delete;
  SplitViewHighlightViewTest& operator=(const SplitViewHighlightViewTest&) =
      delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    left_highlight_ =
        widget_->widget_delegate()->GetContentsView()->AddChildView(
            std::make_unique<SplitViewHighlightView>(false));
    right_highlight_ =
        widget_->widget_delegate()->GetContentsView()->AddChildView(
            std::make_unique<SplitViewHighlightView>(true));
  }

  void SetLeftBounds(const gfx::Rect& bounds, bool animate) {
    SetBounds(bounds, /*is_left=*/true, animate);
  }

  void SetRightBounds(const gfx::Rect& bounds, bool animate) {
    SetBounds(bounds, /*is_left=*/false, animate);
  }

 protected:
  raw_ptr<SplitViewHighlightView, DanglingUntriaged> left_highlight_;
  raw_ptr<SplitViewHighlightView, DanglingUntriaged> right_highlight_;
  std::unique_ptr<views::Widget> widget_;

 private:
  void SetBounds(const gfx::Rect& bounds, bool is_left, bool animate) {
    // The animation type only determines the duration and tween. For testing,
    // any valid animation type would work.
    auto animation_type =
        animate ? std::make_optional(SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN)
                : std::nullopt;
    auto* highlight_view =
        is_left ? left_highlight_.get() : right_highlight_.get();
    highlight_view->SetBounds(bounds, animation_type);
  }
};

TEST_F(SplitViewHighlightViewTest, HighlightGrows) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Tests that before animating, we set the bounds to the desired bounds and
  // clip the rect to the size of the old bounds.
  gfx::Rect start_bounds(100, 100);
  gfx::Rect end_bounds(200, 100);
  SetLeftBounds(start_bounds, /*animate=*/false);
  SetLeftBounds(end_bounds, /*animate=*/true);
  EXPECT_EQ(end_bounds, left_highlight_->bounds());
  EXPECT_EQ(start_bounds, left_highlight_->layer()->clip_rect());

  // After the animation is finished the clip rect should be removed.
  left_highlight_->layer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(gfx::Rect(), left_highlight_->layer()->clip_rect());

  // Tests that for right highlights, the clip is shifted as the animation is
  // mirrored.
  start_bounds = gfx::Rect(100, 0, 100, 100);
  end_bounds = gfx::Rect(200, 100);
  SetRightBounds(start_bounds, /*animate=*/false);
  SetRightBounds(end_bounds, /*animate=*/true);
  EXPECT_EQ(end_bounds, right_highlight_->bounds());
  EXPECT_EQ(start_bounds, right_highlight_->layer()->clip_rect());

  // After the animation is finished the clip rect should be removed.
  right_highlight_->layer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(gfx::Rect(), right_highlight_->layer()->clip_rect());
}

TEST_F(SplitViewHighlightViewTest, HighlightShrinks) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Tests that when the highlight shrinks, the bounds do not get set until the
  // animation is complete.
  gfx::Rect start_bounds(200, 100);
  gfx::Rect end_bounds(100, 100);
  SetLeftBounds(start_bounds, /*animate=*/false);
  SetLeftBounds(end_bounds, /*animate=*/true);
  EXPECT_EQ(start_bounds, left_highlight_->bounds());
  EXPECT_EQ(start_bounds, left_highlight_->layer()->clip_rect());

  // After the animation is finished the clip rect should be removed and the
  // bounds should be set.
  left_highlight_->layer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(end_bounds, left_highlight_->bounds());
  EXPECT_EQ(gfx::Rect(), left_highlight_->layer()->clip_rect());
}

TEST_F(SplitViewHighlightViewTest, PortraitMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Set display to portrait mode.
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);

  ui::ScopedAnimationDurationScaleMode scoped_animation_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  for (bool is_rtl : {false, true}) {
    // RTL should not affect portrait highlights.
    base::test::ScopedRestoreICUDefaultLocale scoped_locale(is_rtl ? "he"
                                                                   : "en_US");
    SCOPED_TRACE(is_rtl ? "RTL" : "LTR");

    // Tests that before animating, we set the bounds to the desired bounds and
    // clip the rect to the size of the old bounds.
    gfx::Rect start_bounds(100, 100);
    gfx::Rect end_bounds(100, 200);
    SetLeftBounds(start_bounds, /*animate=*/false);
    SetLeftBounds(end_bounds, /*animate=*/true);
    EXPECT_EQ(end_bounds, left_highlight_->bounds());
    EXPECT_EQ(start_bounds, left_highlight_->layer()->clip_rect());

    // After the animation is finished the clip rect should be removed.
    left_highlight_->layer()->GetAnimator()->StopAnimating();
    EXPECT_EQ(gfx::Rect(), left_highlight_->layer()->clip_rect());

    // Tests that for bottom highlights, the clip is shifted as the animation is
    // comes from bottom up instead of top down.
    start_bounds = gfx::Rect(0, 100, 100, 100);
    end_bounds = gfx::Rect(200, 100);
    SetRightBounds(start_bounds, /*animate=*/false);
    SetRightBounds(end_bounds, /*animate=*/true);
    EXPECT_EQ(end_bounds, right_highlight_->bounds());
    EXPECT_EQ(start_bounds, right_highlight_->layer()->clip_rect());

    // After the animation is finished the clip rect should be removed.
    right_highlight_->layer()->GetAnimator()->StopAnimating();
    EXPECT_EQ(gfx::Rect(), right_highlight_->layer()->clip_rect());
  }
}

// Tests that the highlights work as in expected in RTL.
TEST_F(SplitViewHighlightViewTest, HighlightInRtl) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale("he");
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // In RTL, the right highlight gets mirrored bounds, so its start and end
  // bounds will have the same origin.
  const gfx::Rect start_bounds(0, 0, 100, 100);
  const gfx::Rect end_bounds(0, 0, 200, 100);
  SetRightBounds(start_bounds, /*animate=*/false);
  SetRightBounds(end_bounds, /*animate=*/true);
  EXPECT_EQ(end_bounds, right_highlight_->bounds());
  EXPECT_EQ(gfx::Rect(100, 0, 100, 100),
            right_highlight_->layer()->clip_rect());

  right_highlight_->layer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(gfx::Rect(), right_highlight_->layer()->clip_rect());
}

}  // namespace ash
