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

class AutoPipSettingViewTest : public views::ViewsTestBase {
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

  using UiResult = AutoPipSettingView::UiResult;

  base::MockOnceCallback<void(UiResult)>& result_cb() { return result_cb_; }
  base::MockOnceCallback<void()>& hide_view_cb() { return hide_view_cb_; }

 private:
  base::MockOnceCallback<void(UiResult)> result_cb_;
  base::MockOnceCallback<void()> hide_view_cb_;
  std::unique_ptr<views::Widget> parent_widget_;
  std::unique_ptr<views::Widget> anchor_view_widget_;
  raw_ptr<AutoPipSettingView> setting_view_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

TEST_F(AutoPipSettingViewTest, TestInitControlViewButton) {
  std::unique_ptr<views::Widget> controls_view_widget = CreateTestWidget();
  controls_view_widget->Show();
  auto* controls_view = controls_view_widget->SetContentsView(
      std::make_unique<views::BoxLayoutView>());

  raw_ptr<views::MdTextButton> test_button =
      setting_view()->InitControlViewButton(
          controls_view, UiResult::kAllowOnEveryVisit, u"Test button");

  EXPECT_EQ(gfx::ALIGN_CENTER, test_button->GetHorizontalAlignment());
  EXPECT_EQ(ui::ButtonStyle::kTonal, test_button->GetStyle());
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
