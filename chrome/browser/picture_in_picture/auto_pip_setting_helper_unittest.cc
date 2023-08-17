// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

class AutoPipSettingHelperTest : public views::ViewsTestBase {
 public:
  AutoPipSettingHelperTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    widget_->Show();

    setting_helper_ =
        std::make_unique<AutoPipSettingHelper>(GURL(), close_cb_.Get());

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(widget_.get()));
  }

  void TearDown() override {
    setting_overlay_ = nullptr;
    widget_.reset();
    setting_helper_.reset();
    ViewsTestBase::TearDown();
  }

  void click_allow() const {
    event_generator_->MoveMouseTo(
        setting_overlay_->get_allow_button_for_testing()
            ->GetBoundsInScreen()
            .CenterPoint());
    event_generator_->ClickLeftButton();
  }

  void click_block() const {
    event_generator_->MoveMouseTo(
        setting_overlay_->get_block_button_for_testing()
            ->GetBoundsInScreen()
            .CenterPoint());
    event_generator_->ClickLeftButton();
  }

  AutoPipSettingHelper* setting_helper() { return setting_helper_.get(); }
  const views::View* setting_overlay() const { return setting_overlay_; }

  base::MockOnceCallback<void()>& close_cb() { return close_cb_; }

  void AttachOverlayView() {
    auto setting_overlay = setting_helper_->CreateOverlayViewIfNeeded();
    if (setting_overlay) {
      setting_overlay_ = static_cast<AutoPipSettingOverlayView*>(
          widget_->SetContentsView(std::move(setting_overlay)));
    }
  }

 private:
  base::MockOnceCallback<void()> close_cb_;

  std::unique_ptr<AutoPipSettingHelper> setting_helper_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AutoPipSettingOverlayView> setting_overlay_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

TEST_F(AutoPipSettingHelperTest, NoUiIfContentSettingIsAllow) {
  setting_helper()->override_content_setting_for_testing(CONTENT_SETTING_ALLOW);

  EXPECT_CALL(close_cb(), Run()).Times(0);
  AttachOverlayView();
  EXPECT_FALSE(setting_overlay());
}

TEST_F(AutoPipSettingHelperTest,
       NoUiButCallbackIsCalledIfContentSettingIsBlock) {
  setting_helper()->override_content_setting_for_testing(CONTENT_SETTING_BLOCK);

  EXPECT_CALL(close_cb(), Run()).Times(1);
  AttachOverlayView();
  EXPECT_FALSE(setting_overlay());
}

TEST_F(AutoPipSettingHelperTest, AllowDoesNotCallCloseCb) {
  setting_helper()->override_content_setting_for_testing(CONTENT_SETTING_ASK);
  AttachOverlayView();
  EXPECT_TRUE(setting_overlay());

  // Click allow.  Nothing should happen.
  EXPECT_CALL(close_cb(), Run()).Times(0);
  click_allow();
}

TEST_F(AutoPipSettingHelperTest, BlockDoesCallCloseCb) {
  setting_helper()->override_content_setting_for_testing(CONTENT_SETTING_ASK);
  AttachOverlayView();
  EXPECT_TRUE(setting_overlay());

  // Click block.  The close cb should be called.
  EXPECT_CALL(close_cb(), Run()).Times(1);
  click_block();
}
