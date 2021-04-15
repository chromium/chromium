// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

class TrayBackgroundViewTestView : public TrayBackgroundView {
 public:
  explicit TrayBackgroundViewTestView(Shelf* shelf)
      : TrayBackgroundView(shelf) {}

  TrayBackgroundViewTestView(const TrayBackgroundViewTestView&) = delete;
  TrayBackgroundViewTestView& operator=(const TrayBackgroundViewTestView&) =
      delete;

  ~TrayBackgroundViewTestView() override = default;

  void ClickedOutsideBubble() override {}
  std::u16string GetAccessibleNameForTray() override {
    return u"TrayBackgroundViewTestView";
  }
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
};

class TrayBackgroundViewTest : public AshTestBase {
 public:
  TrayBackgroundViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  TrayBackgroundViewTest(const TrayBackgroundViewTest&) = delete;
  TrayBackgroundViewTest& operator=(const TrayBackgroundViewTest&) = delete;

  ~TrayBackgroundViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    test_view = std::make_unique<TrayBackgroundViewTestView>(GetPrimaryShelf());

    // Adds this `test_view` to the mock `StatusAreaWidget`. We need to remove
    // the layout manager from the delegate before adding a new child, since
    // there's an DCHECK in the `GridLayout` to assert no more child can be
    // added.
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

 protected:
  std::unique_ptr<TrayBackgroundViewTestView> test_view;
};

TEST_F(TrayBackgroundViewTest, ShowingAnimationAbortedByHideAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Starts showing up animation.
  test_view->SetVisiblePreferred(true);
  EXPECT_TRUE(test_view->GetVisible());
  EXPECT_TRUE(test_view->layer()->GetTargetVisibility());
  EXPECT_TRUE(test_view->layer()->GetAnimator()->is_animating());

  // Starts hide animation. The view is visible but the layer's target
  // visibility is false.
  test_view->SetVisiblePreferred(false);
  EXPECT_TRUE(test_view->GetVisible());
  EXPECT_FALSE(test_view->layer()->GetTargetVisibility());
  EXPECT_TRUE(test_view->layer()->GetAnimator()->is_animating());

  // Here we wait until the animation is finished and we give it one more second
  // to finish the callbacks in `OnVisibilityAnimationFinished()`.
  StatusAreaWidgetTestHelper::WaitForLayerAnimationEnd(test_view->layer());
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // After the hide animation is finished, `test_view` is not visible.
  EXPECT_FALSE(test_view->GetVisible());
  EXPECT_FALSE(test_view->layer()->GetAnimator()->is_animating());
}

}  // namespace ash