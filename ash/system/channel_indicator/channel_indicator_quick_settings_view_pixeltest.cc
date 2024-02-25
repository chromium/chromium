// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator_quick_settings_view.h"
#include "base/memory/raw_ptr.h"

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/system/unified/quick_settings_header.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test_shell_delegate.h"
#include "components/version_info/channel.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ChannelIndicatorQuickSettingsViewPixelTest : public AshTestBase {
 public:
  ChannelIndicatorQuickSettingsViewPixelTest() = default;
  ChannelIndicatorQuickSettingsViewPixelTest(
      const ChannelIndicatorQuickSettingsViewPixelTest&) = delete;
  ChannelIndicatorQuickSettingsViewPixelTest& operator=(
      const ChannelIndicatorQuickSettingsViewPixelTest&) = delete;
  ~ChannelIndicatorQuickSettingsViewPixelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // Install a test delegate to allow overriding channel version.
    auto delegate = std::make_unique<TestShellDelegate>();
    delegate->set_channel(version_info::Channel::BETA);
    AshTestBase::SetUp(std::move(delegate));

    system_tray_client_ = GetSystemTrayClient();
    system_tray_client_->set_user_feedback_enabled(true);

    // Place the view in a large views::Widget so the buttons are clickable.
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
      // Implicitly instantiate the view by creating the quick settings header.
      model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
      controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
      auto header = std::make_unique<QuickSettingsHeader>(controller_.get());
      header_ = header.get();
      widget_->SetContentsView(std::move(header));
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void TearDown() override {
    controller_.reset();
    model_.reset();
    widget_.reset();
    AshTestBase::TearDown();
  }

  QuickSettingsHeader* header() { return header_; }
  ChannelIndicatorQuickSettingsView* view() {
    return header()->channel_view_for_test();
  }

 private:
  raw_ptr<TestSystemTrayClient, DanglingUntriaged> system_tray_client_ =
      nullptr;
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<QuickSettingsHeader, DanglingUntriaged> header_ = nullptr;
};

// Verifies the UI when the feedback button is visible.
TEST_F(ChannelIndicatorQuickSettingsViewPixelTest, FeedbackButtonVisible) {
  // Basic verification that buttons are visible before taking screenshot.
    ASSERT_TRUE(header()->GetVisible());
  ASSERT_TRUE(view());
  ASSERT_TRUE(view()->version_button_for_test());
  ASSERT_TRUE(view()->version_button_for_test()->GetVisible());
  ASSERT_TRUE(view()->feedback_button_for_test());
  ASSERT_TRUE(view()->feedback_button_for_test()->GetVisible());

  // Don't capture any part of the UI except for
  // `ChannelIndicatorQuickSettingsView`.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "feedback_button_visible",
      /*revision_number=*/8, view()));
}

}  // namespace ash
