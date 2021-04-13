// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {

class TrayBackgroundViewTestView : public TrayBackgroundView {
 public:
  explicit TrayBackgroundViewTestView(Shelf* shelf)
      : TrayBackgroundView(shelf) {
    SetVisibility(false);
  }
  ~TrayBackgroundViewTestView() override = default;

  void ClickedOutsideBubble() override {}
  std::u16string GetAccessibleNameForTray() override {
    return u"TrayBackgroundViewTestView";
  }
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {
    aborted = true;
    is_animating = is_starting_animation_;
    OnVisibilityAnimationFinished(true, aborted);
  }

  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    aborted = false;
    is_animating = is_starting_animation_;
    OnVisibilityAnimationFinished(true, aborted);
  }

  bool GetIsStartingAnimation() { return is_starting_animation_; }

  // Sets visibility for playing animation.
  void SetVisibility(bool visible) {
    if (visible) {
      views::View::SetVisible(true);
      layer()->SetVisible(true);
      visible_preferred_ = true;
    } else {
      views::View::SetVisible(false);
      visible_preferred_ = false;
    }
  }

  bool aborted = false;
  bool is_animating = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayBackgroundViewTestView);
};

class TrayBackgroundViewTest : public AshTestBase {
 public:
  TrayBackgroundViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TrayBackgroundViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    test_view = std::make_unique<TrayBackgroundViewTestView>(GetPrimaryShelf());

    // Adds this `test_view` to the mock `StatusAreaWidget`.
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->status_area_widget_delegate()
        ->SetLayoutManager(nullptr);
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()->AddTrayButton(
        test_view.get());
  }

  void TearDown() override {
    test_view.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<TrayBackgroundViewTestView> test_view;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayBackgroundViewTest);
};

TEST_F(TrayBackgroundViewTest, ShowingAnimationAbortedByHideAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  test_view->SetVisibility(false);
  EXPECT_FALSE(test_view->GetIsStartingAnimation());
  EXPECT_FALSE(test_view->GetVisible());

  // Starts showing up animiation.
  test_view->SetVisiblePreferred(true);
  EXPECT_TRUE(test_view->GetVisible());
  EXPECT_FALSE(test_view->GetIsStartingAnimation());
  EXPECT_FALSE(test_view->aborted);
  EXPECT_FALSE(test_view->is_animating);

  // Starts hide animation, by this time showing up animiation should be
  // aborted.
  test_view->SetVisiblePreferred(false);
  EXPECT_TRUE(test_view->GetVisible());
  EXPECT_TRUE(test_view->aborted);
  EXPECT_TRUE(test_view->is_animating);
  EXPECT_FALSE(test_view->GetIsStartingAnimation());

  // Here we wait unitl the animation is finished and we give it one more second
  // to finish the callbacks in `OnVisibilityAnimationFinished`.
  base::RunLoop().RunUntilIdle();
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // After the hide animation is finished, `test_view` is not visible.
  EXPECT_FALSE(test_view->GetVisible());
  EXPECT_FALSE(test_view->GetIsStartingAnimation());
  EXPECT_FALSE(test_view->aborted);
  EXPECT_FALSE(test_view->is_animating);
}

TEST_F(TrayBackgroundViewTest, HideAnimationAbortedByShowingAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  test_view->SetVisibility(true);
  EXPECT_FALSE(test_view->GetIsStartingAnimation());
  EXPECT_TRUE(test_view->GetVisible());

  // Starts hide animiation.
  test_view->SetVisiblePreferred(false);
  EXPECT_TRUE(test_view->GetVisible());
  EXPECT_FALSE(test_view->GetIsStartingAnimation());
  EXPECT_FALSE(test_view->aborted);
  EXPECT_FALSE(test_view->is_animating);

  // Starts showing up animation, by this time hide animiation should be
  // aborted.
  test_view->SetVisiblePreferred(true);
  EXPECT_TRUE(test_view->GetVisible());
  EXPECT_TRUE(test_view->aborted);
  EXPECT_TRUE(test_view->is_animating);
  EXPECT_FALSE(test_view->GetIsStartingAnimation());

  // Here we wait unitl the animation is finished and we give it one more second
  // to finish the callbacks in `OnVisibilityAnimationFinished`.
  base::RunLoop().RunUntilIdle();
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // After the showing animation is finished, `test_view` turns to visible.
  EXPECT_TRUE(test_view->GetVisible());
  EXPECT_FALSE(test_view->GetIsStartingAnimation());
  EXPECT_FALSE(test_view->aborted);
  EXPECT_FALSE(test_view->is_animating);
}

}  // namespace ash
