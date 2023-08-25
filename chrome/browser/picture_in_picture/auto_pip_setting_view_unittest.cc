// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

using ::testing::NotNull;
using UiResult = AutoPipSettingView::UiResult;

struct TestParams {
  UiResult ui_result;
};

class AutoPipSettingViewTest : public views::ViewsTestBase,
                               public testing::WithParamInterface<TestParams> {
 public:
  AutoPipSettingViewTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create parent Widget.
    parent_widget_ = CreateTestWidget();
    parent_widget_->Show();

    // Create the anchor Widget.
    views::Widget::InitParams anchor_view_widget_params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    anchor_view_widget_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    anchor_view_widget_params.bounds = gfx::Rect(200, 200, 50, 10);
    anchor_view_widget_ =
        CreateTestWidget(std::move(anchor_view_widget_params));
    anchor_view_widget_->Show();
    auto* anchor_view =
        anchor_view_widget_->SetContentsView(std::make_unique<views::View>());

    // Define the browser view overridden bounds.
    const gfx::Rect browser_view_overridden_bounds(0, 0, 500, 500);

    // Create the Auto PiP Setting View.
    setting_view_ = new AutoPipSettingView(
        result_cb().Get(), hide_view_cb().Get(), browser_view_overridden_bounds,
        anchor_view, views::BubbleBorder::TOP_CENTER,
        parent_widget_->GetNativeView());
    setting_view_->Show();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(setting_view_->GetWidget()));
  }

  void TearDown() override {
    setting_view_ = nullptr;
    anchor_view_widget_.reset();
    parent_widget_.reset();
    event_generator_.reset();
    ViewsTestBase::TearDown();
  }

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

  const views::Label* autopip_description() const {
    return setting_view_->get_autopip_description_for_testing();
  }

  const views::MdTextButton* allow_once_button() const {
    return setting_view_->get_allow_once_button_for_testing();
  }

  const views::MdTextButton* allow_on_every_visit_button() const {
    return setting_view_->get_allow_on_every_visit_button_button_for_testing();
  }

  const views::MdTextButton* block_button() const {
    return setting_view_->get_block_button_for_testing();
  }

  const AutoPipSettingView* setting_view() const { return setting_view_; }
  AutoPipSettingView* setting_view() { return setting_view_; }

  base::MockOnceCallback<void(UiResult)>& result_cb() { return result_cb_; }
  base::MockOnceCallback<void()>& hide_view_cb() { return hide_view_cb_; }

  const views::MdTextButton* GetButtonForUiResult(UiResult ui_result) const {
    switch (ui_result) {
      case UiResult::kAllowOnce:
        return allow_once_button();
      case UiResult::kAllowOnEveryVisit:
        return allow_on_every_visit_button();
      case UiResult::kBlock:
        return block_button();
      case UiResult::kDismissed:
        return nullptr;
    }
  }

 private:
  base::MockOnceCallback<void(UiResult)> result_cb_;
  base::MockOnceCallback<void()> hide_view_cb_;
  std::unique_ptr<views::Widget> parent_widget_;
  std::unique_ptr<views::Widget> anchor_view_widget_;
  raw_ptr<AutoPipSettingView> setting_view_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

TEST_F(AutoPipSettingViewTest, TestInitControlViewButton) {
  EXPECT_TRUE(setting_view()->GetVisible());
  ASSERT_THAT(allow_once_button(), NotNull());
  EXPECT_EQ(gfx::ALIGN_CENTER, allow_once_button()->GetHorizontalAlignment());
  EXPECT_EQ(ui::ButtonStyle::kTonal, allow_once_button()->GetStyle());
}

TEST_F(AutoPipSettingViewTest, TestSetTitle) {
  EXPECT_TRUE(setting_view()->GetVisible());
  const auto* expected_title =
      u"Sample long title, that should cause the widget size to be adjusted.";
  // Set small size, which should be updated to accommodate the long title.
  const auto initial_widget_size = gfx::Size(10, 10);
  setting_view()->GetWidget()->SetSize(initial_widget_size);
  setting_view()->SetDialogTitle(expected_title);

  EXPECT_FALSE(setting_view()->ShouldCenterWindowTitleText());
  EXPECT_EQ(expected_title, setting_view()->GetWindowTitle());
  EXPECT_GE(
      setting_view()->GetWidget()->GetWindowBoundsInScreen().size().width(),
      initial_widget_size.width());
  EXPECT_GE(
      setting_view()->GetWidget()->GetWindowBoundsInScreen().size().height(),
      initial_widget_size.height());
}

TEST_F(AutoPipSettingViewTest, TestShow) {
  ASSERT_THAT(setting_view()->GetWidget(), NotNull());
  EXPECT_TRUE(setting_view()->GetVisible());
}

TEST_F(AutoPipSettingViewTest, TestViewConstructor) {
  EXPECT_TRUE(setting_view()->GetVisible());
  EXPECT_EQ(views::BubbleBorder::TOP_CENTER, setting_view()->arrow());
  EXPECT_TRUE(setting_view()->use_custom_frame());
}

TEST_F(AutoPipSettingViewTest, VerifyBubbleBorderCustomizations) {
  EXPECT_TRUE(setting_view()->GetVisible());

  // Verify Bubble border customizations.
  EXPECT_EQ(views::BubbleBorder::STANDARD_SHADOW,
            setting_view()->GetBubbleFrameView()->bubble_border()->shadow());
}

const struct TestParams kTestParams[] = {{UiResult::kAllowOnce},
                                         {UiResult::kAllowOnEveryVisit},
                                         {UiResult::kBlock}};

INSTANTIATE_TEST_SUITE_P(AllButtonCallbacks,
                         AutoPipSettingViewTest,
                         testing::ValuesIn(kTestParams));

// Test UiResult callbacks.
TEST_P(AutoPipSettingViewTest, ButtonCallbackTest) {
  EXPECT_TRUE(setting_view()->GetVisible());

  const views::MdTextButton* button_to_test =
      GetButtonForUiResult(GetParam().ui_result);
  if (!button_to_test) {
    return;
  }

  EXPECT_CALL(result_cb(), Run(GetParam().ui_result));

  // Move mouse to the center of the button.
  event_generator()->MoveMouseTo(
      button_to_test->GetBoundsInScreen().CenterPoint());
  event_generator()->ClickLeftButton();

  // Verify that the view is hidden and Widget closed.
  EXPECT_TRUE(setting_view()->GetWidget()->IsClosed());
  EXPECT_FALSE(setting_view()->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(AllMultipleClicks,
                         AutoPipSettingViewTest,
                         testing::ValuesIn(kTestParams));

// Verify that multiple clicks on UI button does not crash.
TEST_P(AutoPipSettingViewTest, MultipleClicksDontCrash) {
  EXPECT_TRUE(setting_view()->GetVisible());

  const views::MdTextButton* button_to_test =
      GetButtonForUiResult(GetParam().ui_result);
  if (!button_to_test) {
    return;
  }

  EXPECT_CALL(result_cb(), Run(GetParam().ui_result));

  // Move mouse to the center of the button.
  event_generator()->MoveMouseTo(
      button_to_test->GetBoundsInScreen().CenterPoint());

  // Perform multiple clicks to verify there are no crashes.
  event_generator()->ClickLeftButton();
  event_generator()->ClickLeftButton();
}

INSTANTIATE_TEST_SUITE_P(AllButtonCallbacksHideOverlayBackgroundLayer,
                         AutoPipSettingViewTest,
                         testing::ValuesIn(kTestParams));

// Verify that the |hide_view_cb_| is executed.
TEST_P(AutoPipSettingViewTest, OverlayBackgroundLayerIsHidden) {
  EXPECT_TRUE(setting_view()->GetVisible());

  const views::MdTextButton* button_to_test =
      GetButtonForUiResult(GetParam().ui_result);
  if (!button_to_test) {
    return;
  }

  EXPECT_CALL(result_cb(), Run(GetParam().ui_result));
  // Verify that the |hide_view_cb_| is executed.
  EXPECT_CALL(hide_view_cb(), Run());

  // Move mouse to the center of the button.
  event_generator()->MoveMouseTo(
      button_to_test->GetBoundsInScreen().CenterPoint());
  event_generator()->ClickLeftButton();
}
