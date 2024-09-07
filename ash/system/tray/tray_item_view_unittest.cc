// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_item_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
constexpr char kShowAnimationSmoothnessHistogramName[] =
    "Ash.StatusArea.TrayItemView.Show";
constexpr char kHideAnimationSmoothnessHistogramName[] =
    "Ash.StatusArea.TrayItemView.Hide";
}  // namespace

// A class that can be used to wait for the given `TrayItemView`'s visibility
// animation to finish (`TrayItemView` does not currently use layer animations,
// so we can't just use `ui::LayerAnimationStoppedWaiter`).
class TrayItemViewAnimationWaiter {
 public:
  explicit TrayItemViewAnimationWaiter(TrayItemView* tray_item)
      : tray_item_(tray_item) {}
  TrayItemViewAnimationWaiter(const TrayItemViewAnimationWaiter&) = delete;
  TrayItemViewAnimationWaiter& operator=(const TrayItemViewAnimationWaiter&) =
      delete;
  ~TrayItemViewAnimationWaiter() = default;

  // Waits for `tray_item_`'s visibility animation to finish, or no-op if it is
  // not currently animating.
  void Wait() {
    if (tray_item_->IsAnimating()) {
      tray_item_->SetAnimationIdleClosureForTest(base::BindOnce(
          &TrayItemViewAnimationWaiter::OnTrayItemAnimationFinished,
          weak_ptr_factory_.GetWeakPtr()));
      run_loop_.Run();
    }
  }

 private:
  // Called when `tray_item_`'s visibility animation finishes.
  void OnTrayItemAnimationFinished() { run_loop_.Quit(); }

  // The tray item whose animation is being waited for. Owned by the views
  // hierarchy.
  raw_ptr<TrayItemView> tray_item_ = nullptr;

  base::RunLoop run_loop_;

  base::WeakPtrFactory<TrayItemViewAnimationWaiter> weak_ptr_factory_{this};
};

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

    // Create a hosting widget with non empty bounds so that it actually draws.
    widget_ = CreateFramelessTestWidget();
    widget_->SetBounds(gfx::Rect(0, 0, 100, 80));
    widget_->Show();
    tray_item_ = widget_->SetContentsView(
        std::make_unique<TestTrayItemView>(GetPrimaryShelf()));
    tray_item_->CreateImageView();
    tray_item_->SetVisible(true);

    // Warms up the compositor so that UI changes are picked up in time before
    // throughput tracker is stopped.
    ui::Compositor* const compositor =
        tray_item()->GetWidget()->GetCompositor();
    compositor->ScheduleFullRedraw();
    ASSERT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  // Helper function that waits not only for `tray_item()`'s animation to finish
  // but also for any animation throughput data to be passed from cc to ui.
  void WaitForAnimation() {
    TrayItemViewAnimationWaiter waiter(tray_item());
    waiter.Wait();

    // Force frames and wait for all throughput trackers to be gone to allow
    // animation throughput data to be passed from cc to ui.
    ui::Compositor* const compositor =
        tray_item()->GetWidget()->GetCompositor();
    while (compositor->has_throughput_trackers_for_testing()) {
      compositor->ScheduleFullRedraw();
      std::ignore = ui::WaitForNextFrameToBePresented(compositor,
                                                      base::Milliseconds(500));
    }
  }

  // Helper function that waits for `tray_item()` opacity to change to a value
  // different from `opacity`.
  void WaitForAnimationChangeOpacityFrom(float opacity) {
    ASSERT_TRUE(tray_item()->IsAnimating());

    ui::Compositor* const compositor =
        tray_item()->GetWidget()->GetCompositor();
    while (tray_item()->layer()->opacity() == opacity) {
      ASSERT_TRUE(ui::WaitForNextFrameToBePresented(compositor));
    }
  }

  views::Widget* widget() { return widget_.get(); }
  TrayItemView* tray_item() { return tray_item_; }

 protected:
  std::unique_ptr<views::Widget> widget_;

  // Owned by `widget`:
  raw_ptr<TrayItemView, DanglingUntriaged> tray_item_ = nullptr;
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
  EXPECT_EQ(tray_item()->CalculatePreferredSize({}), kLargeImageSize);
}

// Tests that a smoothness metric is recorded for the "show" animation.
TEST_F(TrayItemViewTest, SmoothnessMetricRecordedForShowAnimation) {
  // Start with the tray item hidden. Note that animations still complete
  // immediately in this part of the test, so no smoothness metrics are emitted.
  tray_item()->SetVisible(false);
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kHideAnimationSmoothnessHistogramName, 0);

  // Set the animation duration scale to a non-zero value for the rest of the
  // test. Smoothness metrics should be emitted from this point onward.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Start the tray item's "show" animation and wait for it to finish.
  tray_item()->SetVisible(true);
  WaitForAnimation();

  // Verify that the "show" animation's smoothness metric was recorded.
  histogram_tester.ExpectTotalCount(kShowAnimationSmoothnessHistogramName, 1);
}

// Tests that a smoothness metric is recorded for the "hide" animation.
TEST_F(TrayItemViewTest, SmoothnessMetricRecordedForHideAnimation) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kHideAnimationSmoothnessHistogramName, 0);

  // Set the animation duration scale to a non-zero value for the rest of the
  // test. Smoothness metrics should be emitted from this point onward.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Start the tray item's "hide" animation and wait for it to finish.
  tray_item()->SetVisible(false);
  WaitForAnimation();

  // Verify that the "hide" animation's smoothness metric was recorded.
  histogram_tester.ExpectTotalCount(kHideAnimationSmoothnessHistogramName, 1);
}

// Tests that the smoothness metric for the "hide" animation is still recorded
// even when the "hide" animation interrupts the "show" animation.
TEST_F(TrayItemViewTest, HideSmoothnessMetricRecordedWhenHideInterruptsShow) {
  // Start with the tray item hidden. Note that animations still complete
  // immediately in this part of the test, so no smoothness metrics are emitted.
  tray_item()->SetVisible(false);
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kHideAnimationSmoothnessHistogramName, 0);

  // Set the animation duration scale to a non-zero value for the rest of the
  // test. Smoothness metrics should be emitted from this point onward.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Start the tray item's "show" animation, but interrupt it with the "hide"
  // animation. Wait for the "hide" animation to complete.
  tray_item()->SetVisible(true);

  // Wait for animation to change opacity to actually draw on screen. Otherwise,
  // the interrupted animation may end up as a no-op.
  WaitForAnimationChangeOpacityFrom(0.0f);

  tray_item()->SetVisible(false);
  WaitForAnimation();

  // Verify that the "hide" animation's smoothness metric was recorded.
  histogram_tester.ExpectTotalCount(kHideAnimationSmoothnessHistogramName, 1);
}

// Tests that the smoothness metric for the "show" animation is still recorded
// even when the "show" animation interrupts the "hide" animation.
TEST_F(TrayItemViewTest, ShowSmoothnessMetricRecordedWhenShowInterruptsHide) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kHideAnimationSmoothnessHistogramName, 0);

  // Set the animation duration scale to a non-zero value for the rest of the
  // test. Smoothness metrics should be emitted from this point onward.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Start the tray item's "hide" animation, but interrupt it with the "show"
  // animation. Wait for the "show" animation to complete.
  tray_item()->SetVisible(false);

  // Wait for animation to change opacity to actually draw on screen. Otherwise,
  // the interrupted animation may end up as a no-op.
  WaitForAnimationChangeOpacityFrom(1.0f);

  tray_item()->SetVisible(true);
  WaitForAnimation();

  // Verify that the "show" animation's smoothness metric was recorded.
  histogram_tester.ExpectTotalCount(kShowAnimationSmoothnessHistogramName, 1);
}

TEST_F(TrayItemViewTest, IconizedLabelAccessibleProperties) {
  tray_item()->CreateLabel();
  IconizedLabel* label = tray_item()->label();
  ui::AXNodeData data;

  // Test when custom accessible name is empty.
  label->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(label->GetTextContext(), views::style::CONTEXT_LABEL);
  EXPECT_EQ(data.role, ax::mojom::Role::kStaticText);
  EXPECT_FALSE(data.HasStringAttribute(ax::mojom::StringAttribute::kName));

  label->SetText(u"Sample text");
  label->SetTextContext(views::style::CONTEXT_DIALOG_TITLE);
  data = ui::AXNodeData();
  label->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kTitleBar);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Sample text");

  // Test when custom accessible name is not empty.
  label->SetCustomAccessibleName(u"Sample name");
  label->SetTextContext(views::style::CONTEXT_LABEL);
  data = ui::AXNodeData();
  label->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kStaticText);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Sample name");

  label->SetTextContext(views::style::CONTEXT_DIALOG_TITLE);
  label->SetText(u"New sample text");
  data = ui::AXNodeData();
  label->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kStaticText);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Sample name");

  // Test when custom accessible name is again set to empty.
  label->SetCustomAccessibleName(u"");
  data = ui::AXNodeData();
  label->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kTitleBar);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"New sample text");
}

}  // namespace ash
