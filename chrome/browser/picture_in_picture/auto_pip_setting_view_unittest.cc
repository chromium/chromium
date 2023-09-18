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
    setting_view_ = std::make_unique<AutoPipSettingView>(
        result_cb().Get(), hide_view_cb().Get(), browser_view_overridden_bounds,
        anchor_view, views::BubbleBorder::TOP_CENTER);
  }

  void TearDown() override {
    anchor_view_widget_.reset();
    setting_view_.reset();
    ViewsTestBase::TearDown();
  }

  std::unique_ptr<AutoPipSettingView>& setting_view() { return setting_view_; }

  base::MockOnceCallback<void(UiResult)>& result_cb() { return result_cb_; }
  base::MockOnceCallback<void()>& hide_view_cb() { return hide_view_cb_; }

  const views::MdTextButton* GetButtonForUiResult(UiResult ui_result,
                                                  views::Widget* widget) const {
    switch (ui_result) {
      case UiResult::kAllowOnce:
        return GetButton(UiResult::kAllowOnce, widget);
      case UiResult::kAllowOnEveryVisit:
        return GetButton(UiResult::kAllowOnEveryVisit, widget);
      case UiResult::kBlock:
        return GetButton(UiResult::kBlock, widget);
      case UiResult::kDismissed:
        return nullptr;
    }
  }

  void ClickButton(const views::MdTextButton* button_to_test,
                   views::Widget* widget) {
    std::unique_ptr<ui::test::EventGenerator> event_generator =
        std::make_unique<ui::test::EventGenerator>(
            views::GetRootWindow(widget));
    event_generator->MoveMouseTo(
        button_to_test->GetBoundsInScreen().CenterPoint());
    event_generator->ClickLeftButton();
  }

  const views::MdTextButton* GetButton(UiResult button_id,
                                       views::Widget* widget) const {
    return static_cast<views::MdTextButton*>(
        widget->GetContentsView()->GetViewByID(static_cast<int>(button_id)));
  }

 private:
  base::MockOnceCallback<void(UiResult)> result_cb_;
  base::MockOnceCallback<void()> hide_view_cb_;
  std::unique_ptr<views::Widget> anchor_view_widget_;
  std::unique_ptr<AutoPipSettingView> setting_view_;
};

TEST_F(AutoPipSettingViewTest, TestInitControlViewButton) {
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(setting_view()));
  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  const auto* allow_once_button = GetButton(UiResult::kAllowOnce, widget);
  ASSERT_THAT(allow_once_button, NotNull());
  EXPECT_EQ(gfx::ALIGN_CENTER, allow_once_button->GetHorizontalAlignment());
  EXPECT_EQ(ui::ButtonStyle::kTonal, allow_once_button->GetStyle());
}

TEST_F(AutoPipSettingViewTest, TestSetTitle) {
  const auto* expected_title =
      u"Sample long title, that should cause the widget size to be adjusted.";
  // Set small size, which should be updated to accommodate the long title.
  setting_view()->SetDialogTitle(expected_title);
  EXPECT_FALSE(setting_view()->ShouldCenterWindowTitleText());
  EXPECT_EQ(expected_title, setting_view()->GetWindowTitle());
}

TEST_F(AutoPipSettingViewTest, TestShow) {
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(setting_view()));
  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
}

TEST_F(AutoPipSettingViewTest, TestViewConstructor) {
  EXPECT_EQ(views::BubbleBorder::TOP_CENTER, setting_view()->arrow());
  EXPECT_TRUE(setting_view()->use_custom_frame());
}

const struct TestParams kTestParams[] = {{UiResult::kAllowOnce},
                                         {UiResult::kAllowOnEveryVisit},
                                         {UiResult::kBlock}};

INSTANTIATE_TEST_SUITE_P(AllButtonCallbacks,
                         AutoPipSettingViewTest,
                         testing::ValuesIn(kTestParams));

// Test UiResult callbacks.
TEST_P(AutoPipSettingViewTest, ButtonCallbackTest) {
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(setting_view()));
  widget->Show();

  const views::MdTextButton* button_to_test =
      GetButtonForUiResult(GetParam().ui_result, widget);
  if (!button_to_test) {
    return;
  }

  EXPECT_CALL(result_cb(), Run(GetParam().ui_result));

  // Move mouse to the center of the button.
  ClickButton(button_to_test, widget);

  // Verify that the widget closed.
  EXPECT_TRUE(widget->IsClosed());
}

INSTANTIATE_TEST_SUITE_P(AllMultipleClicks,
                         AutoPipSettingViewTest,
                         testing::ValuesIn(kTestParams));

// Verify that multiple clicks on UI button does not crash.
TEST_P(AutoPipSettingViewTest, MultipleClicksDontCrash) {
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(setting_view()));
  widget->Show();

  const views::MdTextButton* button_to_test =
      GetButtonForUiResult(GetParam().ui_result, widget);
  if (!button_to_test) {
    return;
  }

  EXPECT_CALL(result_cb(), Run(GetParam().ui_result));

  // Move mouse to the center of the button.
  ClickButton(button_to_test, widget);

  // Perform multiple clicks to verify there are no crashes.
  ClickButton(button_to_test, widget);
  ClickButton(button_to_test, widget);
}

INSTANTIATE_TEST_SUITE_P(AllButtonCallbacksHideOverlayBackgroundLayer,
                         AutoPipSettingViewTest,
                         testing::ValuesIn(kTestParams));

// Verify that the |hide_view_cb_| is executed.
TEST_P(AutoPipSettingViewTest, OverlayBackgroundLayerIsHidden) {
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(setting_view()));
  widget->Show();

  const views::MdTextButton* button_to_test =
      GetButtonForUiResult(GetParam().ui_result, widget);
  if (!button_to_test) {
    return;
  }

  EXPECT_CALL(result_cb(), Run(GetParam().ui_result));
  // Verify that the |hide_view_cb_| is executed.
  EXPECT_CALL(hide_view_cb(), Run());

  // Move mouse to the center of the button.
  ClickButton(button_to_test, widget);
}
