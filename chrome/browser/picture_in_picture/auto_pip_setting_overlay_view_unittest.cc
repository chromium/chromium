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
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

class AutoPipSettingOverlayViewTest : public views::ViewsTestBase {
 public:
  AutoPipSettingOverlayViewTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create setting overlay widget.
    widget_ = CreateTestWidget();
    widget_->Show();

    // Create parent Widget.
    parent_widget_ = CreateTestWidget();
    parent_widget_->Show();

    // Create the anchor Widget.
    anchor_view_widget_ = CreateTestWidget();
    anchor_view_widget_->Show();
    auto* anchor_view =
        anchor_view_widget_->SetContentsView(std::make_unique<views::View>());

    // Define the browser view overridden bounds.
    const gfx::Rect browser_view_overridden_bounds(0, 0, 500, 500);

    setting_overlay_ =
        widget_->SetContentsView(std::make_unique<AutoPipSettingOverlayView>(
            cb().Get(), origin_, browser_view_overridden_bounds, anchor_view,
            views::BubbleBorder::TOP_CENTER));
  }

  void TearDown() override {
    anchor_view_widget_.reset();
    parent_widget_.reset();
    setting_overlay_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  const AutoPipSettingOverlayView* setting_overlay() const {
    return setting_overlay_;
  }

  const views::View* background() const {
    return setting_overlay_->get_background_for_testing();
  }

  const views::Widget* widget() const { return widget_.get(); }

  using UiResult = AutoPipSettingView::UiResult;

  base::MockOnceCallback<void(UiResult)>& cb() { return cb_; }

 private:
  base::MockOnceCallback<void(UiResult)> cb_;
  std::unique_ptr<views::Widget> parent_widget_;
  std::unique_ptr<views::Widget> anchor_view_widget_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AutoPipSettingOverlayView> setting_overlay_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  const GURL origin_{"https://example.com"};
};

TEST_F(AutoPipSettingOverlayViewTest, TestViewInitialization) {
  EXPECT_TRUE(widget()->IsVisible());
  EXPECT_EQ(
      background()->GetColorProvider()->GetColor(kColorPipWindowBackground),
      background()->GetBackground()->get_color());
}
