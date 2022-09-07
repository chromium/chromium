// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator_quick_settings_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/test/ash_test_base.h"
#include "components/version_info/channel.h"

namespace ash {

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

    // Param is whether user feedback is allowed.
    system_tray_client_ = GetSystemTrayClient();
    system_tray_client_->set_user_feedback_enabled(GetParam());

    // Instantiate view.
    view_ = std::make_unique<ChannelIndicatorQuickSettingsView>(
        version_info::Channel::BETA,
        system_tray_client_->IsUserFeedbackEnabled());
  }

  bool IsFeedbackShown() {
    return system_tray_client_->IsUserFeedbackEnabled();
  }

  ChannelIndicatorQuickSettingsView* view() { return view_.get(); }

 private:
  TestSystemTrayClient* system_tray_client_;
  std::unique_ptr<ChannelIndicatorQuickSettingsView> view_;
};

// Run the `Visible` test below for each value of version_info::Channel.
INSTANTIATE_TEST_SUITE_P(ChannelValues,
                         ChannelIndicatorQuickSettingsViewTest,
                         ::testing::Bool());

TEST_P(ChannelIndicatorQuickSettingsViewTest, Visible) {
  // View exists.
  EXPECT_TRUE(view());

  // Version button is always visible.
  EXPECT_TRUE(view()->IsVersionButtonVisibleForTesting());

  // Feedback button is visible if `SystemTrayClient` says the user preference
  // is set that allows user feedback.
  EXPECT_EQ(view()->IsSubmitFeedbackButtonVisibleForTesting(),
            IsFeedbackShown());
}

}  // namespace ash
