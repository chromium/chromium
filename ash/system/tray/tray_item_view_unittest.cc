// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_item_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class TestTrayItemView : public TrayItemView {
 public:
  explicit TestTrayItemView(Shelf* shelf) : TrayItemView(shelf) {}
  TestTrayItemView(const TestTrayItemView&) = delete;
  TestTrayItemView& operator=(const TestTrayItemView&) = delete;
  ~TestTrayItemView() override = default;

  // TrayItemView:
  void HandleLocaleChange() override {}
};

class TrayItemViewTest : public AshTestBase {
 public:
  TrayItemViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  TrayItemViewTest(const TrayItemViewTest&) = delete;
  TrayItemViewTest& operator=(const TrayItemViewTest&) = delete;
  ~TrayItemViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->Show();
    tray_item_ = widget_->SetContentsView(
        std::make_unique<TestTrayItemView>(GetPrimaryShelf()));
    tray_item_->CreateImageView();
    tray_item_->SetVisible(true);
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  views::Widget* widget() { return widget_.get(); }
  TrayItemView* tray_item() { return tray_item_; }

 protected:
  std::unique_ptr<views::Widget> widget_;

  // Owned by `widget`:
  raw_ptr<TrayItemView, ExperimentalAsh> tray_item_ = nullptr;
};

// Tests that scheduling a `TrayItemView`'s show animation while its hide
// animation is running will stop the hide animation in favor of the show
// animation.
TEST_F(TrayItemViewTest, ShowInterruptsHide) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ASSERT_FALSE(tray_item()->IsAnimating());
  ASSERT_TRUE(tray_item()->GetVisible());

  // Start the tray item's hide animation.
  tray_item()->SetVisible(false);

  // The tray item should be animating to its hide state.
  EXPECT_TRUE(tray_item()->IsAnimating());
  EXPECT_FALSE(tray_item()->target_visible_for_testing());

  // Interrupt the hide animation with the show animation.
  tray_item()->SetVisible(true);

  // The tray item should be animating to its show state.
  EXPECT_TRUE(tray_item()->IsAnimating());
  EXPECT_TRUE(tray_item()->target_visible_for_testing());
}

// Tests that scheduling a `TrayItemView`'s hide animation while its show
// animation is running will stop the show animation in favor of the hide
// animation.
TEST_F(TrayItemViewTest, HideInterruptsShow) {
  // Hide the tray item. Note that at this point in the test animations still
  // complete immediately.
  tray_item()->SetVisible(false);
  ASSERT_FALSE(tray_item()->IsAnimating());
  ASSERT_FALSE(tray_item()->GetVisible());

  // Set the animation duration scale to a non-zero value for the rest of the
  // test.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Start the tray item's show animation.
  tray_item()->SetVisible(true);

  // The tray item should be animating to its show state.
  EXPECT_TRUE(tray_item()->IsAnimating());
  EXPECT_TRUE(tray_item()->target_visible_for_testing());

  // Interrupt the show animation with the hide animation.
  tray_item()->SetVisible(false);

  // The tray item should be animating to its hide state.
  EXPECT_TRUE(tray_item()->IsAnimating());
  EXPECT_FALSE(tray_item()->target_visible_for_testing());
}

// Regression test for http://b/283494045
TEST_F(TrayItemViewTest, ShowDuringZeroDurationAnimation) {
  ui::ScopedAnimationDurationScaleMode duration_scale1(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Hide the tray item and wait for animation to complete.
  base::RunLoop run_loop1;
  tray_item()->SetAnimationIdleClosureForTest(run_loop1.QuitClosure());
  tray_item()->SetVisible(false);
  run_loop1.Run();
  ASSERT_FALSE(tray_item()->IsAnimating());
  ASSERT_FALSE(tray_item()->GetVisible());
  ASSERT_EQ(tray_item()->layer()->opacity(), 0.0f);
  {
    // Set animation duration to zero. The screen rotation animation does this,
    // but it's hard to get that animation into the correct state in a test.
    ui::ScopedAnimationDurationScaleMode duration_scale2(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    // While animations are zero duration, show the item.
    base::RunLoop run_loop2;
    tray_item()->SetAnimationIdleClosureForTest(run_loop2.QuitClosure());
    tray_item()->SetVisible(true);
    run_loop2.Run();
  }

  // The item should be visible and opaque.
  EXPECT_TRUE(tray_item()->GetVisible());
  EXPECT_EQ(tray_item()->layer()->opacity(), 1.0f);
}

TEST_F(TrayItemViewTest, LargeImageIcon) {
  // Use a size that is larger than the default tray icon size.
  const int kLargeSize = 24;
  static_assert(kLargeSize > kUnifiedTrayIconSize);

  // Set the image to a large image.
  gfx::Size kLargeImageSize(kLargeSize, kLargeSize);
  tray_item()->image_view()->SetImage(
      CreateSolidColorTestImage(kLargeImageSize, SK_ColorRED));

  // The preferred size is the size of the larger image (which is not the
  // default tray icon size, see static_assert above).
  EXPECT_EQ(tray_item()->CalculatePreferredSize(), kLargeImageSize);
}

}  // namespace ash
