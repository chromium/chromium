// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/version_info/channel.h"

namespace ash {

class ChannelIndicatorViewTest
    : public NoSessionAshTestBase,
      public testing::WithParamInterface<version_info::Channel> {
 public:
  void SetUp() override {
    // Need this feature enabled in order for the `ChannelIndicatorView` to be
    // instantiated.
    feature_list_.InitAndEnableFeature(features::kReleaseTrackUi);

    // Instantiates the `TestSystemTrayClient`.
    AshTestBase::SetUp();
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
  TestSystemTrayClient* system_tray_client = GetSystemTrayClient();
  DCHECK(system_tray_client);

  // Param is channel value to be set in `system_tray_client`.
  system_tray_client->set_channel(
      static_cast<version_info::Channel>(GetParam()));

  // Now OK to log in to a session.
  constexpr char kUserEmail[] = "user1@test.com";
  SimulateUserLogin(kUserEmail);

  // Local ref.
  UnifiedSystemTray* tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->unified_system_tray();
  ChannelIndicatorView* channel_indicator_view = tray->channel_indicator_view();
  DCHECK(channel_indicator_view);

  // The `ChannelIndicatorView` should be visible for BETA, DEV, and CANARY
  // channels, not visible otherwise.
  switch (system_tray_client->GetChannel()) {
    case version_info::Channel::BETA:
    case version_info::Channel::DEV:
    case version_info::Channel::CANARY:
      EXPECT_TRUE(channel_indicator_view->GetVisible());
      break;
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::STABLE:
      EXPECT_FALSE(channel_indicator_view->GetVisible());
      break;
  }
}

}  // namespace ash
