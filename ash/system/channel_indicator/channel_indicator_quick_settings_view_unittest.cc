// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator_quick_settings_view.h"

#include <tuple>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/version_info/channel.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Parameterized by whether user feedback is enabled.
class ChannelIndicatorQuickSettingsViewTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ChannelIndicatorQuickSettingsViewTest() = default;
  ChannelIndicatorQuickSettingsViewTest(
      const ChannelIndicatorQuickSettingsViewTest&) = delete;
  ChannelIndicatorQuickSettingsViewTest& operator=(
      const ChannelIndicatorQuickSettingsViewTest&) = delete;
  ~ChannelIndicatorQuickSettingsViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Param 1 is whether user feedback is allowed.
    system_tray_client_ = GetSystemTrayClient();
    system_tray_client_->set_user_feedback_enabled(GetParam());

    // Instantiate view.
    auto view = std::make_unique<ChannelIndicatorQuickSettingsView>(
        version_info::Channel::BETA,
        system_tray_client_->IsUserFeedbackEnabled());
    view_ = view.get();

    // Place the view in a large views::Widget so the buttons are clickable.
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(std::move(view));
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  bool IsFeedbackShown() {
    return system_tray_client_->IsUserFeedbackEnabled();
  }

  ChannelIndicatorQuickSettingsView* view() { return view_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<TestSystemTrayClient, DanglingUntriaged> system_tray_client_ =
      nullptr;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<ChannelIndicatorQuickSettingsView, DanglingUntriaged> view_ = nullptr;
};

// Run the `Visible` test below for each value of version_info::Channel.
INSTANTIATE_TEST_SUITE_P(ChannelValues,
                         ChannelIndicatorQuickSettingsViewTest,
                         ::testing::Bool());

TEST_P(ChannelIndicatorQuickSettingsViewTest, Visible) {
  // View exists.
  EXPECT_TRUE(view());

  // Version button is always visible.
  ASSERT_TRUE(view()->version_button_for_test());
  EXPECT_TRUE(view()->version_button_for_test()->GetVisible());

  // Feedback button is visible if `SystemTrayClient` says the user preference
  // is set that allows user feedback.
  views::View* feedback_button = view()->feedback_button_for_test();
  if (IsFeedbackShown()) {
    ASSERT_TRUE(feedback_button);
    EXPECT_TRUE(feedback_button->GetVisible());
  } else {
    ASSERT_FALSE(feedback_button);
  }
}

TEST_P(ChannelIndicatorQuickSettingsViewTest, ClickOnButtons) {
  TestSystemTrayClient* client = GetSystemTrayClient();

  LeftClickOn(view()->version_button_for_test());
  EXPECT_EQ(1, client->show_channel_info_additional_details_count());

  if (IsFeedbackShown()) {
    LeftClickOn(view()->feedback_button_for_test());
    EXPECT_EQ(1, client->show_channel_info_give_feedback_count());
  }
}

}  // namespace ash
