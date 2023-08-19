// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

class AutoPipSettingOverlayViewTest : public views::ViewsTestBase {
 public:
  AutoPipSettingOverlayViewTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    widget_->Show();

    setting_overlay_ = widget_->SetContentsView(
        std::make_unique<AutoPipSettingOverlayView>(cb().Get()));

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(widget_.get()));
  }

  void TearDown() override {
    setting_overlay_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

  const views::View* block_button() const {
    return setting_overlay_->get_block_button_for_testing();
  }

  const views::View* allow_button() const {
    return setting_overlay_->get_allow_button_for_testing();
  }

  const views::View* setting_overlay() const { return setting_overlay_; }

  using UiResult = AutoPipSettingOverlayView::UiResult;

  base::MockOnceCallback<void(UiResult)>& cb() { return cb_; }

 private:
  base::MockOnceCallback<void(UiResult)> cb_;

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AutoPipSettingOverlayView> setting_overlay_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

TEST_F(AutoPipSettingOverlayViewTest, BlockCallsBackWithFalse) {
  EXPECT_CALL(cb(), Run(UiResult::kBlock));
  EXPECT_TRUE(setting_overlay()->GetVisible());
  event_generator()->MoveMouseTo(
      block_button()->GetBoundsInScreen().CenterPoint());
  event_generator()->ClickLeftButton();
  // The button should auto-hide.
  EXPECT_FALSE(setting_overlay()->GetVisible());
}

TEST_F(AutoPipSettingOverlayViewTest, AllowCallsBackWithTrue) {
  EXPECT_CALL(cb(), Run(UiResult::kAllow));
  event_generator()->MoveMouseTo(
      allow_button()->GetBoundsInScreen().CenterPoint());
  event_generator()->ClickLeftButton();
  EXPECT_FALSE(setting_overlay()->GetVisible());
}

TEST_F(AutoPipSettingOverlayViewTest, MultipleClicksDontCrash) {
  EXPECT_CALL(cb(), Run(UiResult::kAllow));
  event_generator()->MoveMouseTo(
      allow_button()->GetBoundsInScreen().CenterPoint());
  event_generator()->ClickLeftButton();
  event_generator()->ClickLeftButton();
  event_generator()->MoveMouseTo(
      block_button()->GetBoundsInScreen().CenterPoint());
  event_generator()->ClickLeftButton();
  event_generator()->ClickLeftButton();
}
