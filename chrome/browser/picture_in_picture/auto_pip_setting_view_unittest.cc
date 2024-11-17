// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/url_formatter/url_formatter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

using ::testing::HasSubstr;
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
        CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    anchor_view_widget_params.bounds = gfx::Rect(200, 200, 50, 10);
    anchor_view_widget_ =
        CreateTestWidget(std::move(anchor_view_widget_params));
    anchor_view_widget_->Show();
    auto* anchor_view =
        anchor_view_widget_->SetContentsView(std::make_unique<views::View>());

    // Create the Auto PiP Setting View.
    setting_view_ = std::make_unique<AutoPipSettingView>(
        result_cb().Get(), hide_view_cb().Get(), origin_, anchor_view,
        views::BubbleBorder::TOP_CENTER);
  }

  void TearDown() override {
    anchor_view_widget_.reset();
    setting_view_.reset();
    ViewsTestBase::TearDown();
  }

  std::unique_ptr<AutoPipSettingView>& setting_view() { return setting_view_; }

  const GURL origin() const { return origin_; }

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
  const GURL origin_{"https://example.com"};
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

TEST_F(AutoPipSettingViewTest, TestBubbleTitleNoElide) {
  // Get the origin text for testing.
  const auto origin_text = setting_view()->get_origin_text_for_testing();

  // Create and show bubble.
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(setting_view()));
  widget->Show();

  // Verify that the bubble title contains the origin.
  EXPECT_EQ(base::UTF16ToUTF8(origin_text), origin().host());
}

TEST_F(AutoPipSettingViewTest, TestBubbleTitleElideBehaviorForNonFileURL) {
  // Set up the setting view.
  const GURL origin{
      "https://example_very_long_url_for_testing_that_should_be_elided.com"};

  auto anchor_view_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  anchor_view_widget->Show();
  auto* anchor_view =
      anchor_view_widget->SetContentsView(std::make_unique<views::View>());

  auto setting_view = std::make_unique<AutoPipSettingView>(
      result_cb().Get(), hide_view_cb().Get(), origin, anchor_view,
      views::BubbleBorder::TOP_CENTER);

  // Get the origin text for testing.
  const auto origin_text = setting_view->get_origin_text_for_testing();

  // Create and show bubble.
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(setting_view));
  widget->Show();

  // Ensure that the origin text has been elided at head.
  EXPECT_TRUE(
      base::StartsWith(origin_text, std::u16string(gfx::kEllipsisUTF16)));
}

TEST_F(AutoPipSettingViewTest, TestBubbleTitleElideBehaviorForFileURL) {
  // Set up the setting view.
  const GURL origin{
      "file://example_very_long_file_url_for_testing_that_should_be_elided"};

  auto anchor_view_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  anchor_view_widget->Show();
  auto* anchor_view =
      anchor_view_widget->SetContentsView(std::make_unique<views::View>());

  auto setting_view = std::make_unique<AutoPipSettingView>(
      result_cb().Get(), hide_view_cb().Get(), origin, anchor_view,
      views::BubbleBorder::TOP_CENTER);

  // Get the origin text for testing.
  const auto origin_text = setting_view->get_origin_text_for_testing();

  // Create and show bubble.
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(setting_view));
  widget->Show();

  // Ensure that the origin text has been elided at tail.
  EXPECT_TRUE(base::EndsWith(origin_text, std::u16string(gfx::kEllipsisUTF16)));
}

TEST_F(AutoPipSettingViewTest, TestOriginLabelForGURLWithLocalHost) {
  // Set up the setting view.
  const GURL origin{
      "file:///example_very_long_file_url_for_testing_that_should_be_elided"};

  auto anchor_view_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  anchor_view_widget->Show();
  auto* anchor_view =
      anchor_view_widget->SetContentsView(std::make_unique<views::View>());

  auto setting_view = std::make_unique<AutoPipSettingView>(
      result_cb().Get(), hide_view_cb().Get(), origin, anchor_view,
      views::BubbleBorder::TOP_CENTER);

  // Get the origin text for testing.
  const auto origin_text = setting_view->get_origin_text_for_testing();

  // Create and show bubble.
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(setting_view));
  widget->Show();

  // Verify that the bubble title contains the URL spec.
  const auto origin_text_without_ellipsis = origin_text.substr(
      0, origin_text.length() - std::u16string(gfx::kEllipsisUTF16).length());
  EXPECT_TRUE(base::StartsWith(base::UTF8ToUTF16(origin.spec()),
                               origin_text_without_ellipsis));
}

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_LINUX)
// TODO (crbug/1521332): Evaluate fix and re-enable
#define MAYBE_WidgetIsCenteredWhenArrowIsFloat \
  DISABLED_WidgetIsCenteredWhenArrowIsFloat
#else
#define MAYBE_WidgetIsCenteredWhenArrowIsFloat WidgetIsCenteredWhenArrowIsFloat
#endif
TEST_F(AutoPipSettingViewTest, MAYBE_WidgetIsCenteredWhenArrowIsFloat) {
  // Set up the anchor view.
  views::Widget::InitParams anchor_view_widget_params =
      CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  anchor_view_widget_params.bounds = gfx::Rect(200, 200, 700, 700);
  auto anchor_view_widget =
      CreateTestWidget(std::move(anchor_view_widget_params));
  auto* anchor_view =
      anchor_view_widget->SetContentsView(std::make_unique<views::View>());
  anchor_view_widget->Show();

  // Set up the setting view.
  auto setting_view = std::make_unique<AutoPipSettingView>(
      result_cb().Get(), hide_view_cb().Get(), GURL("https://example.com"),
      anchor_view, views::BubbleBorder::FLOAT);

  // Create and show bubble.
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(setting_view));
  widget->Show();

  // Get the anchor view and widget bounds.
  const auto anchor_bounds = anchor_view->GetBoundsInScreen();
  const auto widget_bounds = widget->GetWindowBoundsInScreen();

  // Calculate left and right distance between the widget and anchor view.
  const auto left_distance = widget_bounds.x() - anchor_bounds.x();
  const auto right_distance = anchor_bounds.right() - widget_bounds.right();

  // Calculate top and bottom distance between the widget and anchor view.
  const auto top_distance = widget_bounds.y() - anchor_bounds.y();
  const auto bottom_distance = anchor_bounds.bottom() - widget_bounds.bottom();

  // Verify that the widget is centered, relative to the anchor view.
  EXPECT_EQ(left_distance, right_distance);
  EXPECT_EQ(top_distance, bottom_distance);
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
