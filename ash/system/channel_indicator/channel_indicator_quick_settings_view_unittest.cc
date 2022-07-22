// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator_quick_settings_view.h"

#include "ash/constants/ash_features.h"
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

    // Instantiate members.
    view_ = std::make_unique<ChannelIndicatorQuickSettingsView>(
        static_cast<version_info::Channel>(GetParam()));
  }

  // Ignored for now, will come into play with the fix for crbug.com/1344855.
  bool IsFeedbackShown() { return GetParam(); }

  ChannelIndicatorQuickSettingsView* view() { return view_.get(); }

 private:
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

  // Feedback button is always visible, for now. This will change with the fix
  // for crbug.com/1344855.
  EXPECT_TRUE(view()->IsSubmitFeedbackButtonVisibleForTesting());
}

}  // namespace ash
