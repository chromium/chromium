// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/channel_indicator/channel_indicator_utils.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/test/scoped_feature_list.h"
#include "components/session_manager/session_manager_types.h"
#include "components/version_info/channel.h"

namespace ash {

class ChannelIndicatorViewTest
    : public AshTestBase,
      public testing::WithParamInterface<version_info::Channel> {
 public:
  ChannelIndicatorViewTest() = default;
  ChannelIndicatorViewTest(const ChannelIndicatorViewTest&) = delete;
  ChannelIndicatorViewTest& operator=(const ChannelIndicatorViewTest&) = delete;
  ~ChannelIndicatorViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // Need this feature enabled in order for the `ChannelIndicatorView` to be
    // instantiated.
    feature_list_.InitAndEnableFeature(features::kReleaseTrackUi);

    // Instantiate a `TestShellDelegate` with the channel set to our param.
    std::unique_ptr<TestShellDelegate> shell_delegate =
        std::make_unique<TestShellDelegate>();
    shell_delegate->set_channel(static_cast<version_info::Channel>(GetParam()));
    AshTestBase::SetUp(std::move(shell_delegate));
  }

  void SetSessionState(session_manager::SessionState state) {
    SessionInfo info;
    info.state = state;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Run the `Visible` test below for each value of version_info::Channel.
INSTANTIATE_TEST_SUITE_P(ChannelValues,
                         ChannelIndicatorViewTest,
                         ::testing::Values(version_info::Channel::UNKNOWN,
                                           version_info::Channel::STABLE,
                                           version_info::Channel::BETA,
                                           version_info::Channel::DEV,
                                           version_info::Channel::CANARY));

TEST_P(ChannelIndicatorViewTest, Visible) {
  // Local ref.
  ShellDelegate* shell_delegate = Shell::Get()->shell_delegate();
  DCHECK(shell_delegate);

  // Local ref.
  UnifiedSystemTray* tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->unified_system_tray();
  ChannelIndicatorView* channel_indicator_view = tray->channel_indicator_view();
  DCHECK(channel_indicator_view);

  // The `ChannelIndicatorView` should be visible for BETA, DEV, and CANARY
  // channels, not visible otherwise.
  EXPECT_EQ(channel_indicator_view->GetVisible(),
            channel_indicator_utils::IsDisplayableChannel(
                shell_delegate->GetChannel()));

  // If the view is visible, verify what we display depending on the session
  // state.
  if (channel_indicator_view->GetVisible()) {
    // User is not logged in, view should display text, no image.
    SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);
    EXPECT_TRUE(channel_indicator_view->IsLabelVisibleForTesting());
    EXPECT_FALSE(channel_indicator_view->IsImageViewVisibleForTesting());

    // User is logged in, view should display image, no text.
    SetSessionState(session_manager::SessionState::ACTIVE);
    EXPECT_FALSE(channel_indicator_view->IsLabelVisibleForTesting());
    EXPECT_TRUE(channel_indicator_view->IsImageViewVisibleForTesting());

    // User locks the session, view should display text, no image.
    SetSessionState(session_manager::SessionState::LOCKED);
    EXPECT_TRUE(channel_indicator_view->IsLabelVisibleForTesting());
    EXPECT_FALSE(channel_indicator_view->IsImageViewVisibleForTesting());

    // User is logged in again, view should display image, no text.
    SetSessionState(session_manager::SessionState::ACTIVE);
    EXPECT_FALSE(channel_indicator_view->IsLabelVisibleForTesting());
    EXPECT_TRUE(channel_indicator_view->IsImageViewVisibleForTesting());
  }
}

}  // namespace ash
