// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_utils.h"

namespace {

class MockWidgetObserver : public views::WidgetObserver {
 public:
  MOCK_METHOD(void, OnWidgetClosing, (views::Widget*), ());
};

}  // namespace

class AutoPipSettingOverlayViewTest : public views::ViewsTestBase {
 public:
  AutoPipSettingOverlayViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create setting overlay widget.
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget_->Show();

    // Create parent Widget.
    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    parent_widget_->Show();

    // Create the anchor Widget.
    anchor_view_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    anchor_view_widget_->Show();
    auto* anchor_view =
        anchor_view_widget_->SetContentsView(std::make_unique<views::View>());

    animation_duration_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    setting_overlay_ =
        widget_->SetContentsView(std::make_unique<AutoPipSettingOverlayView>(
            cb().Get(), origin_, anchor_view, views::BubbleBorder::TOP_CENTER));
  }

  void TearDown() override {
    animation_duration_.reset();
    anchor_view_widget_.reset();
    parent_widget_.reset();
    setting_overlay_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  AutoPipSettingOverlayView* setting_overlay() { return setting_overlay_; }

  views::Widget* anchor_view_widget() { return anchor_view_widget_.get(); }

  views::View* background() const {
    return setting_overlay_->get_background_for_testing();
  }

  views::View* blur_view() const {
    return setting_overlay_->get_blur_view_for_testing();
  }

  const views::Widget* widget() const { return widget_.get(); }

  using UiResult = AutoPipSettingView::UiResult;

  base::MockOnceCallback<void(UiResult)>& cb() { return cb_; }

  void RemoveAndDeleteSettingOverlay() {
    setting_overlay_ = nullptr;

    // Setting the contents view will remove and delete the existing contents
    // view, which is the setting overlay.
    widget_->SetContentsView(std::make_unique<views::View>());
  }

 protected:
  void WaitForBubbleToBeShown() {
    // Bubble should be shown after an 500Ms delay.
    task_environment()->FastForwardBy(base::Milliseconds(505));
  }

 private:
  base::MockOnceCallback<void(UiResult)> cb_;
  std::unique_ptr<views::Widget> parent_widget_;
  std::unique_ptr<views::Widget> anchor_view_widget_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AutoPipSettingOverlayView> setting_overlay_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  const GURL origin_{"https://example.com"};

  // Used to force a non-zero animation duration.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> animation_duration_;
};

TEST_F(AutoPipSettingOverlayViewTest, TestViewInitialization) {
  EXPECT_TRUE(widget()->IsVisible());
  EXPECT_EQ(
      background()->GetColorProvider()->GetColor(kColorPipWindowBackground),
      background()->GetBackground()->get_color());
  EXPECT_EQ(4.0f, blur_view()->layer()->background_blur());
}

TEST_F(AutoPipSettingOverlayViewTest, TestBackgroundLayerAnimation) {
  // Background layer opacity should start at 0.0f and end at 0.60f.
  EXPECT_EQ(0.0f, background()->layer()->opacity());
  EXPECT_EQ(0.60f, background()->layer()->GetTargetOpacity());

  // Progress animation to its end position. Background layer should fade in to
  // a 0.60f opacity.
  background()->layer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(0.60f, background()->layer()->opacity());
}

TEST_F(AutoPipSettingOverlayViewTest, TestWantsEvent) {
  setting_overlay()->ShowBubble(anchor_view_widget()->GetNativeView());
  // Assume nothing is at screen coordinate 0,0.
  EXPECT_FALSE(setting_overlay()->WantsEvent(gfx::Point(0, 0)));

  // Bubble show timer is running (bubble has not been shown yet). Make sure
  // that clicking any of the overlay view buttons returns false.
  auto* view = setting_overlay()->get_view_for_testing();
  EXPECT_FALSE(setting_overlay()->WantsEvent(
      view->get_allow_always_button_center_in_screen_for_testing()));
  EXPECT_FALSE(setting_overlay()->WantsEvent(
      view->get_allow_once_button_center_in_screen_for_testing()));
  EXPECT_FALSE(setting_overlay()->WantsEvent(
      view->get_block_button_center_in_screen_for_testing()));

  // Bubble show timer is no longer running (bubble is now visible). Make sure
  // that the buttons work.
  WaitForBubbleToBeShown();
  EXPECT_TRUE(setting_overlay()->WantsEvent(
      view->get_allow_always_button_center_in_screen_for_testing()));
  EXPECT_TRUE(setting_overlay()->WantsEvent(
      view->get_allow_once_button_center_in_screen_for_testing()));
  EXPECT_TRUE(setting_overlay()->WantsEvent(
      view->get_block_button_center_in_screen_for_testing()));
}

TEST_F(AutoPipSettingOverlayViewTest,
       TestOverlayViewDoesNotHaveFocusForDocumentPip) {
  setting_overlay()->ShowBubble(anchor_view_widget()->GetNativeView());
  EXPECT_FALSE(setting_overlay()->HasFocus());
}

TEST_F(AutoPipSettingOverlayViewTest,
       TestOverlayViewDoesNotHaveFocusForVideoPip) {
  setting_overlay()->ShowBubble(anchor_view_widget()->GetNativeView());
  EXPECT_FALSE(setting_overlay()->HasFocus());
}

#if BUILDFLAG(IS_MAC)
// Ensure that bubbles requested for Document Picture-in-Picture windows are
// shown not shown before an 180Ms delay, on Mac.
TEST_F(AutoPipSettingOverlayViewTest,
       TestOverlayViewNotShownBeforeDelayForDocumentPip) {
  // Initially bubble should not be shown.
  setting_overlay()->ShowBubble(anchor_view_widget()->GetNativeView());
  EXPECT_FALSE(
      setting_overlay()->get_view_for_testing()->GetWidget()->IsVisible());

  // Bubble should not be shown before an 180Ms delay. This is to ensure that
  // the Mac window animation has completed, before we show the permission
  // prompt.
  task_environment()->FastForwardBy(base::Milliseconds(179));
  EXPECT_FALSE(
      setting_overlay()->get_view_for_testing()->GetWidget()->IsVisible());
}
#endif  // BUILDFLAG(IS_MAC)

// Ensure that bubbles requested for Document Picture-in-Picture windows are
// shown after the scrim animation.
TEST_F(AutoPipSettingOverlayViewTest, TestOverlayViewShownWithDelay) {
  setting_overlay()->ShowBubble(anchor_view_widget()->GetNativeView());
  EXPECT_FALSE(
      setting_overlay()->get_view_for_testing()->GetWidget()->IsVisible());

  WaitForBubbleToBeShown();
  EXPECT_TRUE(
      setting_overlay()->get_view_for_testing()->GetWidget()->IsVisible());
}

TEST_F(AutoPipSettingOverlayViewTest, TestDeletingOverlayClosesBubble) {
  setting_overlay()->ShowBubble(anchor_view_widget()->GetNativeView());

  MockWidgetObserver widget_observer;
  views::Widget* widget =
      setting_overlay()->get_view_for_testing()->GetWidget();
  widget->AddObserver(&widget_observer);

  // Removing and deleting the setting overlay should close the bubble widget.
  EXPECT_CALL(widget_observer, OnWidgetClosing(widget))
      .WillOnce(testing::InvokeWithoutArgs(
          [&]() { widget->RemoveObserver(&widget_observer); }));
  RemoveAndDeleteSettingOverlay();
  testing::Mock::VerifyAndClearExpectations(&widget_observer);
}

namespace {

class TestAutoPipSettingOverlayViewDelegate
    : public AutoPipSettingOverlayView::Delegate {
 public:
  explicit TestAutoPipSettingOverlayViewDelegate(
      AutoPipSettingOverlayView* overlay_view)
      : overlay_view_(overlay_view) {
    overlay_view->set_delegate(this);
  }
  TestAutoPipSettingOverlayViewDelegate(
      const TestAutoPipSettingOverlayViewDelegate&) = delete;
  TestAutoPipSettingOverlayViewDelegate& operator=(
      const TestAutoPipSettingOverlayViewDelegate&) = delete;

  ~TestAutoPipSettingOverlayViewDelegate() override {
    overlay_view_->set_delegate(nullptr);
  }

  bool observer_notified() const { return observer_notified_; }

  void OnAutoPipSettingOverlayViewHidden() override {
    observer_notified_ = true;
  }

 private:
  const raw_ptr<AutoPipSettingOverlayView> overlay_view_ = nullptr;
  bool observer_notified_ = false;
};

}  // namespace

TEST_F(AutoPipSettingOverlayViewTest, TestAutoPipSettingOverlayViewDelegate) {
  // Set up observer and show bubble.
  TestAutoPipSettingOverlayViewDelegate auto_pip_setting_overlay_view_observer(
      setting_overlay());
  setting_overlay()->ShowBubble(anchor_view_widget()->GetNativeView());
  WaitForBubbleToBeShown();

  // Delegate should not have been notified at this point, since overlay view
  // has not been hidden.
  EXPECT_FALSE(auto_pip_setting_overlay_view_observer.observer_notified());

  // Simulate clicking the "Allow once" button, which causes the overlay view to
  // be hidden.
  std::unique_ptr<ui::test::EventGenerator> event_generator =
      std::make_unique<ui::test::EventGenerator>(views::GetRootWindow(
          setting_overlay()->get_view_for_testing()->GetWidget()));
  event_generator->MoveMouseTo(
      setting_overlay()
          ->get_view_for_testing()
          ->get_allow_once_button_center_in_screen_for_testing());
  event_generator->ClickLeftButton();

  // Ensure that observer was notified.
  EXPECT_TRUE(auto_pip_setting_overlay_view_observer.observer_notified());
}
